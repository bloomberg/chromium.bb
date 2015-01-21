# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import pickle
import re
import sys

from pylib import cmd_helper
from pylib import constants
from pylib import flag_changer
from pylib.base import base_test_result
from pylib.base import test_instance
from pylib.instrumentation import test_result
from pylib.utils import apk_helper
from pylib.utils import md5sum
from pylib.utils import proguard

sys.path.append(
    os.path.join(constants.DIR_SOURCE_ROOT, 'build', 'util', 'lib', 'common'))
import unittest_util

_DEFAULT_ANNOTATIONS = [
    'Smoke', 'SmallTest', 'MediumTest', 'LargeTest',
    'EnormousTest', 'IntegrationTest']
_NATIVE_CRASH_RE = re.compile('native crash', re.IGNORECASE)
_PICKLE_FORMAT_VERSION = 10


# TODO(jbudorick): Make these private class methods of
# InstrumentationTestInstance once the instrumentation test_runner is
# deprecated.
def ParseAmInstrumentRawOutput(raw_output):
  """Parses the output of an |am instrument -r| call.

  Args:
    raw_output: the output of an |am instrument -r| call as a list of lines
  Returns:
    A 3-tuple containing:
      - the instrumentation code as an integer
      - the instrumentation result as a list of lines
      - the instrumentation statuses received as a list of 2-tuples
        containing:
        - the status code as an integer
        - the bundle dump as a dict mapping string keys to a list of
          strings, one for each line.
  """
  INSTR_STATUS = 'INSTRUMENTATION_STATUS: '
  INSTR_STATUS_CODE = 'INSTRUMENTATION_STATUS_CODE: '
  INSTR_RESULT = 'INSTRUMENTATION_RESULT: '
  INSTR_CODE = 'INSTRUMENTATION_CODE: '

  last = None
  instr_code = None
  instr_result = []
  instr_statuses = []
  bundle = {}
  for line in raw_output:
    if line.startswith(INSTR_STATUS):
      instr_var = line[len(INSTR_STATUS):]
      if '=' in instr_var:
        k, v = instr_var.split('=', 1)
        bundle[k] = [v]
        last = INSTR_STATUS
        last_key = k
      else:
        logging.debug('Unknown "%s" line: %s' % (INSTR_STATUS, line))

    elif line.startswith(INSTR_STATUS_CODE):
      instr_status = line[len(INSTR_STATUS_CODE):]
      instr_statuses.append((int(instr_status), bundle))
      bundle = {}
      last = INSTR_STATUS_CODE

    elif line.startswith(INSTR_RESULT):
      instr_result.append(line[len(INSTR_RESULT):])
      last = INSTR_RESULT

    elif line.startswith(INSTR_CODE):
      instr_code = int(line[len(INSTR_CODE):])
      last = INSTR_CODE

    elif last == INSTR_STATUS:
      bundle[last_key].append(line)

    elif last == INSTR_RESULT:
      instr_result.append(line)

  return (instr_code, instr_result, instr_statuses)


def GenerateTestResult(test_name, instr_statuses, start_ms, duration_ms):
  """Generate the result of |test| from |instr_statuses|.

  Args:
    test_name: The name of the test as "class#method"
    instr_statuses: A list of 2-tuples containing:
      - the status code as an integer
      - the bundle dump as a dict mapping string keys to string values
      Note that this is the same as the third item in the 3-tuple returned by
      |_ParseAmInstrumentRawOutput|.
    start_ms: The start time of the test in milliseconds.
    duration_ms: The duration of the test in milliseconds.
  Returns:
    An InstrumentationTestResult object.
  """
  INSTR_STATUS_CODE_START = 1
  INSTR_STATUS_CODE_OK = 0
  INSTR_STATUS_CODE_ERROR = -1
  INSTR_STATUS_CODE_FAIL = -2

  log = ''
  result_type = base_test_result.ResultType.UNKNOWN

  for status_code, bundle in instr_statuses:
    if status_code == INSTR_STATUS_CODE_START:
      pass
    elif status_code == INSTR_STATUS_CODE_OK:
      bundle_test = '%s#%s' % (
          ''.join(bundle.get('class', [''])),
          ''.join(bundle.get('test', [''])))
      skipped = ''.join(bundle.get('test_skipped', ['']))

      if (test_name == bundle_test and
          result_type == base_test_result.ResultType.UNKNOWN):
        result_type = base_test_result.ResultType.PASS
      elif skipped.lower() in ('true', '1', 'yes'):
        result_type = base_test_result.ResultType.SKIP
        logging.info('Skipped ' + test_name)
    else:
      if status_code not in (INSTR_STATUS_CODE_ERROR,
                             INSTR_STATUS_CODE_FAIL):
        logging.error('Unrecognized status code %d. Handling as an error.',
                      status_code)
      result_type = base_test_result.ResultType.FAIL
      if 'stack' in bundle:
        log = '\n'.join(bundle['stack'])

  return test_result.InstrumentationTestResult(
      test_name, result_type, start_ms, duration_ms, log=log)


class InstrumentationTestInstance(test_instance.TestInstance):

  def __init__(self, args, isolate_delegate, error_func):
    super(InstrumentationTestInstance, self).__init__()

    self._apk_under_test = None
    self._package_info = None
    self._test_apk = None
    self._test_jar = None
    self._test_package = None
    self._test_runner = None
    self._test_support_apk = None
    self.__initializeApkAttributes(args, error_func)

    self._data_deps = None
    self._isolate_abs_path = None
    self._isolate_delegate = None
    self._isolated_abs_path = None
    self._test_data = None
    self.__initializeDataDependencyAttributes(args, isolate_delegate)

    self._annotations = None
    self._excluded_annotations = None
    self._test_filter = None
    self.__initializeTestFilterAttributes(args)

    self._flags = None
    self.__initializeFlagAttributes(args)

  def __initializeApkAttributes(self, args, error_func):
    if args.apk_under_test.endswith('.apk'):
      self._apk_under_test = args.apk_under_test
    else:
      self._apk_under_test = os.path.join(
          constants.GetOutDirectory(), constants.SDK_BUILD_APKS_DIR,
          '%s.apk' % args.apk_under_test)

    if not os.path.exists(self._apk_under_test):
      error_func('Unable to find APK under test: %s' % self._apk_under_test)

    if args.test_apk.endswith('.apk'):
      test_apk_root = os.path.splitext(os.path.basename(args.test_apk))[0]
      self._test_apk = args.test_apk
    else:
      test_apk_root = args.test_apk
      self._test_apk = os.path.join(
          constants.GetOutDirectory(), constants.SDK_BUILD_APKS_DIR,
          '%s.apk' % args.test_apk)

    self._test_jar = os.path.join(
        constants.GetOutDirectory(), constants.SDK_BUILD_TEST_JAVALIB_DIR,
        '%s.jar' % test_apk_root)
    self._test_support_apk = os.path.join(
        constants.GetOutDirectory(), constants.SDK_BUILD_TEST_JAVALIB_DIR,
        '%sSupport.apk' % test_apk_root)

    if not os.path.exists(self._test_apk):
      error_func('Unable to find test APK: %s' % self._test_apk)
    if not os.path.exists(self._test_jar):
      error_func('Unable to find test JAR: %s' % self._test_jar)

    self._test_package = apk_helper.GetPackageName(self.test_apk)
    self._test_runner = apk_helper.GetInstrumentationName(self.test_apk)

    self._package_info = None
    for package_info in constants.PACKAGE_INFO.itervalues():
      if self._test_package == package_info.test_package:
        self._package_info = package_info
    if not self._package_info:
      error_func('Unable to find package info for %s' % self._test_package)

  def __initializeDataDependencyAttributes(self, args, isolate_delegate):
    self._data_deps = []
    if args.isolate_file_path:
      self._isolate_abs_path = os.path.abspath(args.isolate_file_path)
      self._isolate_delegate = isolate_delegate
      self._isolated_abs_path = os.path.join(
          constants.GetOutDirectory(), '%s.isolated' % self._test_package)
    else:
      self._isolate_delegate = None

    # TODO(jbudorick): Deprecate and remove --test-data once data dependencies
    # are fully converted to isolate.
    if args.test_data:
      logging.info('Data dependencies specified via --test-data')
      self._test_data = args.test_data
    else:
      self._test_data = None

    if not self._isolate_delegate and not self._test_data:
      logging.warning('No data dependencies will be pushed.')

  def __initializeTestFilterAttributes(self, args):
    self._test_filter = args.test_filter

    def annotation_dict_element(a):
      a = a.split('=')
      return (a[0], a[1] if len(a) == 2 else None)

    if args.annotation_str:
      self._annotations = dict(
          annotation_dict_element(a)
          for a in args.annotation_str.split(','))
    elif not self._test_filter:
      self._annotations = dict(
          annotation_dict_element(a)
          for a in _DEFAULT_ANNOTATIONS)
    else:
      self._annotations = {}

    if args.exclude_annotation_str:
      self._excluded_annotations = dict(
          annotation_dict_element(a)
          for a in args.exclude_annotation_str.split(','))
    else:
      self._excluded_annotations = {}

  def __initializeFlagAttributes(self, args):
    self._flags = ['--disable-fre', '--enable-test-intents']
    # TODO(jbudorick): Transition "--device-flags" to "--device-flags-file"
    if hasattr(args, 'device_flags') and args.device_flags:
      with open(args.device_flags) as device_flags_file:
        stripped_lines = (l.strip() for l in device_flags_file)
        self._flags.extend([flag for flag in stripped_lines if flag])
    if hasattr(args, 'device_flags_file') and args.device_flags_file:
      with open(args.device_flags_file) as device_flags_file:
        stripped_lines = (l.strip() for l in device_flags_file)
        self._flags.extend([flag for flag in stripped_lines if flag])

  @property
  def suite(self):
    return 'instrumentation'

  @property
  def apk_under_test(self):
    return self._apk_under_test

  @property
  def flags(self):
    return self._flags

  @property
  def package_info(self):
    return self._package_info

  @property
  def test_apk(self):
    return self._test_apk

  @property
  def test_jar(self):
    return self._test_jar

  @property
  def test_support_apk(self):
    return self._test_support_apk

  @property
  def test_package(self):
    return self._test_package

  @property
  def test_runner(self):
    return self._test_runner

  #override
  def TestType(self):
    return 'instrumentation'

  #override
  def SetUp(self):
    if self._isolate_delegate:
      self._isolate_delegate.Remap(
          self._isolate_abs_path, self._isolated_abs_path)
      self._isolate_delegate.MoveOutputDeps()
      self._data_deps.extend([(constants.ISOLATE_DEPS_DIR, None)])

    # TODO(jbudorick): Convert existing tests that depend on the --test-data
    # mechanism to isolate, then remove this.
    if self._test_data:
      for t in self._test_data:
        device_rel_path, host_rel_path = t.split(':')
        host_abs_path = os.path.join(constants.DIR_SOURCE_ROOT, host_rel_path)
        self._data_deps.extend(
            [(host_abs_path,
              [None, 'chrome', 'test', 'data', device_rel_path])])

  def GetDataDependencies(self):
    return self._data_deps

  def GetTests(self):
    pickle_path = '%s-proguard.pickle' % self.test_jar
    try:
      tests = self._GetTestsFromPickle(pickle_path, self.test_jar)
    except self.ProguardPickleException as e:
      logging.info('Getting tests from JAR via proguard. (%s)' % str(e))
      tests = self._GetTestsFromProguard(self.test_jar)
      self._SaveTestsToPickle(pickle_path, self.test_jar, tests)
    return self._InflateTests(self._FilterTests(tests))

  class ProguardPickleException(Exception):
    pass

  def _GetTestsFromPickle(self, pickle_path, jar_path):
    if not os.path.exists(pickle_path):
      raise self.ProguardPickleException('%s does not exist.' % pickle_path)
    if os.path.getmtime(pickle_path) <= os.path.getmtime(jar_path):
      raise self.ProguardPickleException(
          '%s newer than %s.' % (jar_path, pickle_path))

    with open(pickle_path, 'r') as pickle_file:
      pickle_data = pickle.loads(pickle_file.read())
    jar_md5, _ = md5sum.CalculateHostMd5Sums(jar_path)[0]

    try:
      if pickle_data['VERSION'] != _PICKLE_FORMAT_VERSION:
        raise self.ProguardPickleException('PICKLE_FORMAT_VERSION has changed.')
      if pickle_data['JAR_MD5SUM'] != jar_md5:
        raise self.ProguardPickleException('JAR file MD5 sum differs.')
      return pickle_data['TEST_METHODS']
    except TypeError as e:
      logging.error(pickle_data)
      raise self.ProguardPickleException(str(e))

  def _GetTestsFromProguard(self, jar_path):
    p = proguard.Dump(jar_path)

    def is_test_class(c):
      return c['class'].endswith('Test')

    def is_test_method(m):
      return m['method'].startswith('test')

    class_lookup = dict((c['class'], c) for c in p['classes'])
    def recursive_get_class_annotations(c):
      s = c['superclass']
      if s in class_lookup:
        a = recursive_get_class_annotations(class_lookup[s])
      else:
        a = {}
      a.update(c['annotations'])
      return a

    def stripped_test_class(c):
      return {
        'class': c['class'],
        'annotations': recursive_get_class_annotations(c),
        'methods': [m for m in c['methods'] if is_test_method(m)],
      }

    return [stripped_test_class(c) for c in p['classes']
            if is_test_class(c)]

  def _SaveTestsToPickle(self, pickle_path, jar_path, tests):
    jar_md5, _ = md5sum.CalculateHostMd5Sums(jar_path)[0]
    pickle_data = {
      'VERSION': _PICKLE_FORMAT_VERSION,
      'JAR_MD5SUM': jar_md5,
      'TEST_METHODS': tests,
    }
    with open(pickle_path, 'w') as pickle_file:
      pickle.dump(pickle_data, pickle_file)

  def _FilterTests(self, tests):

    def gtest_filter(c, m):
      t = ['%s.%s' % (c['class'].split('.')[-1], m['method'])]
      return (not self._test_filter
              or unittest_util.FilterTestNames(t, self._test_filter))

    def annotation_filter(all_annotations):
      if not self._annotations:
        return True
      return any_annotation_matches(self._annotations, all_annotations)

    def excluded_annotation_filter(all_annotations):
      if not self._excluded_annotations:
        return True
      return not any_annotation_matches(self._excluded_annotations,
                                        all_annotations)

    def any_annotation_matches(annotations, all_annotations):
      return any(
          ak in all_annotations and (av is None or av == all_annotations[ak])
          for ak, av in annotations.iteritems())

    filtered_classes = []
    for c in tests:
      filtered_methods = []
      for m in c['methods']:
        # Gtest filtering
        if not gtest_filter(c, m):
          continue

        all_annotations = dict(c['annotations'])
        all_annotations.update(m['annotations'])
        if (not annotation_filter(all_annotations)
            or not excluded_annotation_filter(all_annotations)):
          continue

        filtered_methods.append(m)

      if filtered_methods:
        filtered_class = dict(c)
        filtered_class['methods'] = filtered_methods
        filtered_classes.append(filtered_class)

    return filtered_classes

  def _InflateTests(self, tests):
    inflated_tests = []
    for c in tests:
      for m in c['methods']:
        a = dict(c['annotations'])
        a.update(m['annotations'])
        inflated_tests.append({
            'class': c['class'],
            'method': m['method'],
            'annotations': a,
        })
    return inflated_tests

  @staticmethod
  def GenerateMultiTestResult(errors, statuses):
    INSTR_STATUS_CODE_START = 1
    results = []
    skip_counter = 1
    for status_code, bundle in statuses:
      if status_code != INSTR_STATUS_CODE_START:
        # TODO(rnephew): Make skipped tests still output test name. This is only
        # there to give skipped tests a unique name so they are counted
        if 'test_skipped' in bundle:
          test_name = str(skip_counter)
          skip_counter += 1
        else:
          test_name = '%s#%s' % (
              ''.join(bundle.get('class', [''])),
              ''.join(bundle.get('test', [''])))

        results.append(
            GenerateTestResult(test_name, [(status_code, bundle)], 0, 0))
    for error in errors:
      if _NATIVE_CRASH_RE.search(error):
        results.append(
            base_test_result.BaseTestResult(
            'Crash detected', base_test_result.ResultType.CRASH))

    return results

  @staticmethod
  def ParseAmInstrumentRawOutput(raw_output):
    return ParseAmInstrumentRawOutput(raw_output)

  @staticmethod
  def GenerateTestResult(test_name, instr_statuses, start_ms, duration_ms):
    return GenerateTestResult(test_name, instr_statuses, start_ms, duration_ms)

  #override
  def TearDown(self):
    if self._isolate_delegate:
      self._isolate_delegate.Clear()

