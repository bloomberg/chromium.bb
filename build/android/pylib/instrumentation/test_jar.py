# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Helper class for instrumenation test jar."""
# pylint: disable=W0702

import logging
import os
import pickle
import re
import sys
import tempfile

from pylib import cmd_helper
from pylib import constants
from pylib.device import device_utils

sys.path.insert(0,
                os.path.join(constants.DIR_SOURCE_ROOT,
                             'build', 'util', 'lib', 'common'))

import unittest_util # pylint: disable=F0401

# If you change the cached output of proguard, increment this number
PICKLE_FORMAT_VERSION = 2


class TestJar(object):
  _ANNOTATIONS = frozenset(
      ['Smoke', 'SmallTest', 'MediumTest', 'LargeTest', 'EnormousTest',
       'FlakyTest', 'DisabledTest', 'Manual', 'PerfTest', 'HostDrivenTest',
       'IntegrationTest'])
  _DEFAULT_ANNOTATION = 'SmallTest'
  _PROGUARD_CLASS_RE = re.compile(r'\s*?- Program class:\s*([\S]+)$')
  _PROGUARD_SUPERCLASS_RE = re.compile(r'\s*?  Superclass:\s*([\S]+)$')
  _PROGUARD_METHOD_RE = re.compile(r'\s*?- Method:\s*(\S*)[(].*$')
  _PROGUARD_ANNOTATION_RE = re.compile(r'\s*?- Annotation \[L(\S*);\]:$')
  _PROGUARD_ANNOTATION_CONST_RE = (
      re.compile(r'\s*?- Constant element value.*$'))
  _PROGUARD_ANNOTATION_VALUE_RE = re.compile(r'\s*?- \S+? \[(.*)\]$')

  def __init__(self, jar_path):
    if not os.path.exists(jar_path):
      raise Exception('%s not found, please build it' % jar_path)

    self._PROGUARD_PATH = os.path.join(constants.ANDROID_SDK_ROOT,
                                       'tools/proguard/lib/proguard.jar')
    if not os.path.exists(self._PROGUARD_PATH):
      self._PROGUARD_PATH = os.path.join(os.environ['ANDROID_BUILD_TOP'],
                                         'external/proguard/lib/proguard.jar')
    self._jar_path = jar_path
    self._pickled_proguard_name = self._jar_path + '-proguard.pickle'
    self._test_methods = {}
    if not self._GetCachedProguardData():
      self._GetProguardData()

  def _GetCachedProguardData(self):
    if (os.path.exists(self._pickled_proguard_name) and
        (os.path.getmtime(self._pickled_proguard_name) >
         os.path.getmtime(self._jar_path))):
      logging.info('Loading cached proguard output from %s',
                   self._pickled_proguard_name)
      try:
        with open(self._pickled_proguard_name, 'r') as r:
          d = pickle.loads(r.read())
        if d['VERSION'] == PICKLE_FORMAT_VERSION:
          self._test_methods = d['TEST_METHODS']
          return True
      except:
        logging.warning('PICKLE_FORMAT_VERSION has changed, ignoring cache')
    return False

  def _GetProguardData(self):
    logging.info('Retrieving test methods via proguard.')

    with tempfile.NamedTemporaryFile() as proguard_output:
      cmd_helper.RunCmd(['java', '-jar',
                         self._PROGUARD_PATH,
                         '-injars', self._jar_path,
                         '-dontshrink',
                         '-dontoptimize',
                         '-dontobfuscate',
                         '-dontpreverify',
                         '-dump', proguard_output.name])

      clazzez = {}

      annotation = None
      annotation_has_value = False
      clazz = None
      method = None

      for line in proguard_output:
        if len(line) == 0:
          annotation = None
          annotation_has_value = False
          method = None
          continue

        m = self._PROGUARD_CLASS_RE.match(line)
        if m:
          clazz = m.group(1).replace('/', '.')
          clazzez[clazz] = {
            'methods': {},
            'annotations': {}
          }
          annotation = None
          annotation_has_value = False
          method = None
          continue

        if not clazz:
          continue

        m = self._PROGUARD_SUPERCLASS_RE.match(line)
        if m:
          clazzez[clazz]['superclass'] = m.group(1).replace('/', '.')
          continue

        if clazz.endswith('Test'):
          m = self._PROGUARD_METHOD_RE.match(line)
          if m:
            method = m.group(1)
            clazzez[clazz]['methods'][method] = {'annotations': {}}
            annotation = None
            annotation_has_value = False
            continue

        m = self._PROGUARD_ANNOTATION_RE.match(line)
        if m:
          # Ignore the annotation package.
          annotation = m.group(1).split('/')[-1]
          if method:
            clazzez[clazz]['methods'][method]['annotations'][annotation] = None
          else:
            clazzez[clazz]['annotations'][annotation] = None
          continue

        if annotation:
          if not annotation_has_value:
            m = self._PROGUARD_ANNOTATION_CONST_RE.match(line)
            annotation_has_value = bool(m)
          else:
            m = self._PROGUARD_ANNOTATION_VALUE_RE.match(line)
            if m:
              if method:
                clazzez[clazz]['methods'][method]['annotations'][annotation] = (
                    m.group(1))
              else:
                clazzez[clazz]['annotations'][annotation] = m.group(1)
            annotation_has_value = None

    test_clazzez = ((n, i) for n, i in clazzez.items() if n.endswith('Test'))
    for clazz_name, clazz_info in test_clazzez:
      logging.info('Processing %s' % clazz_name)
      c = clazz_name
      min_sdk_level = None

      while c in clazzez:
        c_info = clazzez[c]
        if not min_sdk_level:
          min_sdk_level = c_info['annotations'].get('MinAndroidSdkLevel', None)
        c = c_info.get('superclass', None)

      for method_name, method_info in clazz_info['methods'].items():
        if method_name.startswith('test'):
          qualified_method = '%s#%s' % (clazz_name, method_name)
          method_annotations = []
          for annotation_name, annotation_value in (
              method_info['annotations'].items()):
            method_annotations.append(annotation_name)
            if annotation_value:
              method_annotations.append(
                  annotation_name + ':' + annotation_value)
          self._test_methods[qualified_method] = {
            'annotations': method_annotations
          }
          if min_sdk_level is not None:
            self._test_methods[qualified_method]['min_sdk_level'] = (
                int(min_sdk_level))

    logging.info('Storing proguard output to %s', self._pickled_proguard_name)
    d = {'VERSION': PICKLE_FORMAT_VERSION,
         'TEST_METHODS': self._test_methods}
    with open(self._pickled_proguard_name, 'w') as f:
      f.write(pickle.dumps(d))


  @staticmethod
  def _IsTestMethod(test):
    class_name, method = test.split('#')
    return class_name.endswith('Test') and method.startswith('test')

  def GetTestAnnotations(self, test):
    """Returns a list of all annotations for the given |test|. May be empty."""
    if not self._IsTestMethod(test) or not test in self._test_methods:
      return []
    return self._test_methods[test]['annotations']

  @staticmethod
  def _AnnotationsMatchFilters(annotation_filter_list, annotations):
    """Checks if annotations match any of the filters."""
    if not annotation_filter_list:
      return True
    for annotation_filter in annotation_filter_list:
      filters = annotation_filter.split('=')
      if len(filters) == 2:
        key = filters[0]
        value_list = filters[1].split(',')
        for value in value_list:
          if key + ':' + value in annotations:
            return True
      elif annotation_filter in annotations:
        return True
    return False

  def GetAnnotatedTests(self, annotation_filter_list):
    """Returns a list of all tests that match the given annotation filters."""
    return [test for test, attr in self.GetTestMethods().iteritems()
            if self._IsTestMethod(test) and self._AnnotationsMatchFilters(
                annotation_filter_list, attr['annotations'])]

  def GetTestMethods(self):
    """Returns a dict of all test methods and relevant attributes.

    Test methods are retrieved as Class#testMethod.
    """
    return self._test_methods

  def _GetTestsMissingAnnotation(self):
    """Get a list of test methods with no known annotations."""
    tests_missing_annotations = []
    for test_method in self.GetTestMethods().iterkeys():
      annotations_ = frozenset(self.GetTestAnnotations(test_method))
      if (annotations_.isdisjoint(self._ANNOTATIONS) and
          not self.IsHostDrivenTest(test_method)):
        tests_missing_annotations.append(test_method)
    return sorted(tests_missing_annotations)

  def _IsTestValidForSdkRange(self, test_name, attached_min_sdk_level):
    required_min_sdk_level = self.GetTestMethods()[test_name].get(
        'min_sdk_level', None)
    return (required_min_sdk_level is None or
            attached_min_sdk_level >= required_min_sdk_level)

  def GetAllMatchingTests(self, annotation_filter_list,
                          exclude_annotation_list, test_filter):
    """Get a list of tests matching any of the annotations and the filter.

    Args:
      annotation_filter_list: List of test annotations. A test must have at
        least one of these annotations. A test without any annotations is
        considered to be SmallTest.
      exclude_annotation_list: List of test annotations. A test must not have
        any of these annotations.
      test_filter: Filter used for partial matching on the test method names.

    Returns:
      List of all matching tests.
    """
    if annotation_filter_list:
      available_tests = self.GetAnnotatedTests(annotation_filter_list)
      # Include un-annotated tests in SmallTest.
      if annotation_filter_list.count(self._DEFAULT_ANNOTATION) > 0:
        for test in self._GetTestsMissingAnnotation():
          logging.warning(
              '%s has no annotations. Assuming "%s".', test,
              self._DEFAULT_ANNOTATION)
          available_tests.append(test)
      if exclude_annotation_list:
        excluded_tests = self.GetAnnotatedTests(exclude_annotation_list)
        available_tests = list(set(available_tests) - set(excluded_tests))
    else:
      available_tests = [m for m in self.GetTestMethods()
                         if not self.IsHostDrivenTest(m)]

    tests = []
    if test_filter:
      # |available_tests| are in adb instrument format: package.path.class#test.

      # Maps a 'class.test' name to each 'package.path.class#test' name.
      sanitized_test_names = dict([
          (t.split('.')[-1].replace('#', '.'), t) for t in available_tests])
      # Filters 'class.test' names and populates |tests| with the corresponding
      # 'package.path.class#test' names.
      tests = [
          sanitized_test_names[t] for t in unittest_util.FilterTestNames(
              sanitized_test_names.keys(), test_filter.replace('#', '.'))]
    else:
      tests = available_tests

    # Filter out any tests with SDK level requirements that don't match the set
    # of attached devices.
    sdk_versions = [
        int(v) for v in
        device_utils.DeviceUtils.parallel().GetProp(
            'ro.build.version.sdk').pGet(None)]
    tests = filter(
        lambda t: self._IsTestValidForSdkRange(t, min(sdk_versions)),
        tests)

    return tests

  @staticmethod
  def IsHostDrivenTest(test):
    return 'pythonDrivenTests' in test
