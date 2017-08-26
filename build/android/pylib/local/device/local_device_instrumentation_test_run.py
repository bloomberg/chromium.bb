# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import contextlib
import hashlib
import json
import logging
import os
import posixpath
import re
import sys
import tempfile
import time

from devil.android import crash_handler
from devil.android import device_errors
from devil.android import device_temp_file
from devil.android import flag_changer
from devil.android.sdk import shared_prefs
from devil.android.tools import system_app
from devil.utils import reraiser_thread
from incremental_install import installer
from pylib import valgrind_tools
from pylib.android import logdog_logcat_monitor
from pylib.base import base_test_result
from pylib.constants import host_paths
from pylib.instrumentation import instrumentation_test_instance
from pylib.local.device import local_device_environment
from pylib.local.device import local_device_test_run
from pylib.utils import google_storage_helper
from pylib.utils import instrumentation_tracing
from pylib.utils import logdog_helper
from pylib.utils import shared_preference_utils
from py_trace_event import trace_event
from py_trace_event import trace_time
from py_utils import contextlib_ext
from py_utils import tempfile_ext
import tombstones

with host_paths.SysPath(
    os.path.join(host_paths.DIR_SOURCE_ROOT, 'third_party'), 0):
  import jinja2  # pylint: disable=import-error
  import markupsafe  # pylint: disable=import-error,unused-import


_JINJA_TEMPLATE_DIR = os.path.join(
    host_paths.DIR_SOURCE_ROOT, 'build', 'android', 'pylib', 'instrumentation')
_JINJA_TEMPLATE_FILENAME = 'render_test.html.jinja'

_TAG = 'test_runner_py'

TIMEOUT_ANNOTATIONS = [
  ('Manual', 10 * 60 * 60),
  ('IntegrationTest', 30 * 60),
  ('External', 10 * 60),
  ('EnormousTest', 10 * 60),
  ('LargeTest', 5 * 60),
  ('MediumTest', 3 * 60),
  ('SmallTest', 1 * 60),
]

LOGCAT_FILTERS = ['*:e', 'chromium:v', 'cr_*:v', 'DEBUG:I',
                  'StrictMode:D', '%s:I' % _TAG]

EXTRA_SCREENSHOT_FILE = (
    'org.chromium.base.test.ScreenshotOnFailureStatement.ScreenshotFile')

EXTRA_UI_CAPTURE_DIR = (
    'org.chromium.base.test.util.Screenshooter.ScreenshotDir')

EXTRA_TRACE_FILE = ('org.chromium.base.test.BaseJUnit4ClassRunner.TraceFile')

_EXTRA_TEST_LIST = (
    'org.chromium.base.test.BaseChromiumAndroidJUnitRunner.TestList')

_TEST_LIST_JUNIT4_RUNNERS = [
    'org.chromium.base.test.BaseChromiumAndroidJUnitRunner']

UI_CAPTURE_DIRS = ['chromium_tests_root', 'UiCapture']

FEATURE_ANNOTATION = 'Feature'
RENDER_TEST_FEATURE_ANNOTATION = 'RenderTest'

# This needs to be kept in sync with formatting in |RenderUtils.imageName|
RE_RENDER_IMAGE_NAME = re.compile(
      r'(?P<test_class>\w+)\.'
      r'(?P<description>[-\w]+)\.'
      r'(?P<device_model_sdk>[-\w]+)\.png')

@contextlib.contextmanager
def _LogTestEndpoints(device, test_name):
  device.RunShellCommand(
      ['log', '-p', 'i', '-t', _TAG, 'START %s' % test_name],
      check_return=True)
  try:
    yield
  finally:
    device.RunShellCommand(
        ['log', '-p', 'i', '-t', _TAG, 'END %s' % test_name],
        check_return=True)

# TODO(jbudorick): Make this private once the instrumentation test_runner
# is deprecated.
def DidPackageCrashOnDevice(package_name, device):
  # Dismiss any error dialogs. Limit the number in case we have an error
  # loop or we are failing to dismiss.
  try:
    for _ in xrange(10):
      package = device.DismissCrashDialogIfNeeded()
      if not package:
        return False
      # Assume test package convention of ".test" suffix
      if package in package_name:
        return True
  except device_errors.CommandFailedError:
    logging.exception('Error while attempting to dismiss crash dialog.')
  return False


_CURRENT_FOCUS_CRASH_RE = re.compile(
    r'\s*mCurrentFocus.*Application (Error|Not Responding): (\S+)}')


class LocalDeviceInstrumentationTestRun(
    local_device_test_run.LocalDeviceTestRun):
  def __init__(self, env, test_instance):
    super(LocalDeviceInstrumentationTestRun, self).__init__(env, test_instance)
    self._flag_changers = {}
    self._ui_capture_dir = dict()
    self._replace_package_contextmanager = None

  #override
  def TestPackage(self):
    return self._test_instance.suite

  #override
  def SetUp(self):
    @local_device_environment.handle_shard_failures_with(
        self._env.BlacklistDevice)
    @trace_event.traced
    def individual_device_set_up(device, host_device_tuples):
      steps = []

      if self._test_instance.replace_system_package:
        @trace_event.traced
        def replace_package(dev):
          # We need the context manager to be applied before modifying any
          # shared preference files in case the replacement APK needs to be
          # set up, and it needs to be applied while the test is running.
          # Thus, it needs to be applied early during setup, but must still be
          # applied during _RunTest, which isn't possible using 'with' without
          # applying the context manager up in test_runner. Instead, we
          # manually invoke its __enter__ and __exit__ methods in setup and
          # teardown.
          self._replace_package_contextmanager = system_app.ReplaceSystemApp(
              dev, self._test_instance.replace_system_package.package,
              self._test_instance.replace_system_package.replacement_apk)
          self._replace_package_contextmanager.__enter__()

        steps.append(replace_package)

      def install_helper(apk, permissions):
        @instrumentation_tracing.no_tracing
        @trace_event.traced("apk_path")
        def install_helper_internal(d, apk_path=apk.path):
          # pylint: disable=unused-argument
          d.Install(apk, permissions=permissions)
        return install_helper_internal

      def incremental_install_helper(apk, json_path):
        @trace_event.traced("apk_path")
        def incremental_install_helper_internal(d, apk_path=apk.path):
          # pylint: disable=unused-argument
          installer.Install(d, json_path, apk=apk)
        return incremental_install_helper_internal

      if self._test_instance.apk_under_test:
        if self._test_instance.apk_under_test_incremental_install_json:
          steps.append(incremental_install_helper(
                           self._test_instance.apk_under_test,
                           self._test_instance.
                               apk_under_test_incremental_install_json))
        else:
          permissions = self._test_instance.apk_under_test.GetPermissions()
          steps.append(install_helper(self._test_instance.apk_under_test,
                                      permissions))

      if self._test_instance.test_apk_incremental_install_json:
        steps.append(incremental_install_helper(
                         self._test_instance.test_apk,
                         self._test_instance.
                             test_apk_incremental_install_json))
      else:
        permissions = self._test_instance.test_apk.GetPermissions()
        steps.append(install_helper(self._test_instance.test_apk,
                                    permissions))

      steps.extend(install_helper(apk, None)
                   for apk in self._test_instance.additional_apks)

      @trace_event.traced
      def set_debug_app(dev):
        # Set debug app in order to enable reading command line flags on user
        # builds
        if self._test_instance.flags:
          if not self._test_instance.package_info:
            logging.error("Couldn't set debug app: no package info")
          elif not self._test_instance.package_info.package:
            logging.error("Couldn't set debug app: no package defined")
          else:
            dev.RunShellCommand(['am', 'set-debug-app', '--persistent',
                               self._test_instance.package_info.package],
                              check_return=True)

      @trace_event.traced
      def edit_shared_prefs(dev):
        for setting in self._test_instance.edit_shared_prefs:
          shared_pref = shared_prefs.SharedPrefs(dev, setting['package'],
                                                 setting['filename'])
          shared_preference_utils.ApplySharedPreferenceSetting(
              shared_pref, setting)

      @instrumentation_tracing.no_tracing
      def push_test_data(dev):
        device_root = posixpath.join(dev.GetExternalStoragePath(),
                                     'chromium_tests_root')
        host_device_tuples_substituted = [
            (h, local_device_test_run.SubstituteDeviceRoot(d, device_root))
            for h, d in host_device_tuples]
        logging.info('instrumentation data deps:')
        for h, d in host_device_tuples_substituted:
          logging.info('%r -> %r', h, d)
        dev.PushChangedFiles(host_device_tuples_substituted,
                             delete_device_stale=True)
        if not host_device_tuples_substituted:
          dev.RunShellCommand(['rm', '-rf', device_root], check_return=True)
          dev.RunShellCommand(['mkdir', '-p', device_root], check_return=True)

      @trace_event.traced
      def create_flag_changer(dev):
        if self._test_instance.flags:
          if not self._test_instance.package_info:
            logging.error("Couldn't set flags: no package info")
          elif not self._test_instance.package_info.cmdline_file:
            logging.error("Couldn't set flags: no cmdline_file")
          else:
            self._CreateFlagChangerIfNeeded(dev)
            logging.debug('Attempting to set flags: %r',
                          self._test_instance.flags)
            self._flag_changers[str(dev)].AddFlags(self._test_instance.flags)

        valgrind_tools.SetChromeTimeoutScale(
            dev, self._test_instance.timeout_scale)

      @trace_event.traced
      def setup_ui_capture_dir(dev):
        # Make sure the UI capture directory exists and is empty by deleting
        # and recreating it.
        # TODO (aberent) once DeviceTempDir exists use it here.
        self._ui_capture_dir[dev] = posixpath.join(
            dev.GetExternalStoragePath(),
            *UI_CAPTURE_DIRS)

        if dev.PathExists(self._ui_capture_dir[dev]):
          dev.RunShellCommand(['rm', '-rf', self._ui_capture_dir[dev]])
        dev.RunShellCommand(['mkdir', self._ui_capture_dir[dev]])

      steps += [set_debug_app, edit_shared_prefs, push_test_data,
                create_flag_changer, setup_ui_capture_dir]

      def bind_crash_handler(step, dev):
        return lambda: crash_handler.RetryOnSystemCrash(step, dev)

      steps = [bind_crash_handler(s, device) for s in steps]

      if self._env.concurrent_adb:
        reraiser_thread.RunAsync(steps)
      else:
        for step in steps:
          step()
      if self._test_instance.store_tombstones:
        tombstones.ClearAllTombstones(device)

    self._env.parallel_devices.pMap(
        individual_device_set_up,
        self._test_instance.GetDataDependencies())

  #override
  def TearDown(self):
    @local_device_environment.handle_shard_failures_with(
        self._env.BlacklistDevice)
    @trace_event.traced
    def individual_device_tear_down(dev):
      if str(dev) in self._flag_changers:
        self._flag_changers[str(dev)].Restore()

      # Remove package-specific configuration
      dev.RunShellCommand(['am', 'clear-debug-app'], check_return=True)

      valgrind_tools.SetChromeTimeoutScale(dev, None)

      if self._test_instance.ui_screenshot_dir:
        pull_ui_screen_captures(dev)

      if self._replace_package_contextmanager:
        self._replace_package_contextmanager.__exit__(*sys.exc_info())

    @trace_event.traced
    def pull_ui_screen_captures(dev):
      file_names = dev.ListDirectory(self._ui_capture_dir[dev])
      target_path = self._test_instance.ui_screenshot_dir
      if not os.path.exists(target_path):
        os.makedirs(target_path)

      for file_name in file_names:
        dev.PullFile(posixpath.join(self._ui_capture_dir[dev], file_name),
                     target_path)

    self._env.parallel_devices.pMap(individual_device_tear_down)

  def _CreateFlagChangerIfNeeded(self, device):
    if not str(device) in self._flag_changers:
      self._flag_changers[str(device)] = flag_changer.FlagChanger(
        device, self._test_instance.package_info.cmdline_file)

  #override
  def _CreateShards(self, tests):
    return tests

  #override
  def _GetTests(self):
    tests = None
    if self._test_instance.junit4_runner_class in _TEST_LIST_JUNIT4_RUNNERS:
      raw_tests = self._GetTestsFromRunner()
      tests = self._test_instance.ProcessRawTests(raw_tests)
    else:
      tests = self._test_instance.GetTests()
    tests = self._ApplyExternalSharding(
        tests, self._test_instance.external_shard_index,
        self._test_instance.total_external_shards)
    return tests

  #override
  def _GetUniqueTestName(self, test):
    return instrumentation_test_instance.GetUniqueTestName(test)

  #override
  def _RunTest(self, device, test):
    extras = {}

    flags_to_add = []
    test_timeout_scale = None
    if self._test_instance.coverage_directory:
      coverage_basename = '%s.ec' % ('%s_group' % test[0]['method']
          if isinstance(test, list) else test['method'])
      extras['coverage'] = 'true'
      coverage_directory = os.path.join(
          device.GetExternalStoragePath(), 'chrome', 'test', 'coverage')
      coverage_device_file = os.path.join(
          coverage_directory, coverage_basename)
      extras['coverageFile'] = coverage_device_file
    # Save screenshot if screenshot dir is specified (save locally) or if
    # a GS bucket is passed (save in cloud).
    screenshot_device_file = None
    if (self._test_instance.screenshot_dir or
        self._test_instance.gs_results_bucket):
      screenshot_device_file = device_temp_file.DeviceTempFile(
          device.adb, suffix='.png', dir=device.GetExternalStoragePath())
      extras[EXTRA_SCREENSHOT_FILE] = screenshot_device_file.name

    extras[EXTRA_UI_CAPTURE_DIR] = self._ui_capture_dir[device]

    if self._env.trace_output:
      trace_device_file = device_temp_file.DeviceTempFile(
          device.adb, suffix='.json', dir=device.GetExternalStoragePath())
      extras[EXTRA_TRACE_FILE] = trace_device_file.name

    if isinstance(test, list):
      if not self._test_instance.driver_apk:
        raise Exception('driver_apk does not exist. '
                        'Please build it and try again.')
      if any(t.get('is_junit4') for t in test):
        raise Exception('driver apk does not support JUnit4 tests')

      def name_and_timeout(t):
        n = instrumentation_test_instance.GetTestName(t)
        i = self._GetTimeoutFromAnnotations(t['annotations'], n)
        return (n, i)

      test_names, timeouts = zip(*(name_and_timeout(t) for t in test))

      test_name = ','.join(test_names)
      test_display_name = test_name
      target = '%s/%s' % (
          self._test_instance.driver_package,
          self._test_instance.driver_name)
      extras.update(
          self._test_instance.GetDriverEnvironmentVars(
              test_list=test_names))
      timeout = sum(timeouts)
    else:
      test_name = instrumentation_test_instance.GetTestName(test)
      test_display_name = self._GetUniqueTestName(test)
      if test['is_junit4']:
        target = '%s/%s' % (
            self._test_instance.test_package,
            self._test_instance.junit4_runner_class)
      else:
        target = '%s/%s' % (
            self._test_instance.test_package,
            self._test_instance.junit3_runner_class)
      extras['class'] = test_name
      if 'flags' in test and test['flags']:
        flags_to_add.extend(test['flags'])
      timeout = self._GetTimeoutFromAnnotations(
        test['annotations'], test_display_name)

      test_timeout_scale = self._GetTimeoutScaleFromAnnotations(
          test['annotations'])
      if test_timeout_scale and test_timeout_scale != 1:
        valgrind_tools.SetChromeTimeoutScale(
            device, test_timeout_scale * self._test_instance.timeout_scale)

    logging.info('preparing to run %s: %s', test_display_name, test)

    render_tests_device_output_dir = None
    if _IsRenderTest(test):
      # TODO(mikecase): Add DeviceTempDirectory class and use that instead.
      render_tests_device_output_dir = posixpath.join(
          device.GetExternalStoragePath(),
          'render_test_output_dir')
      flags_to_add.append('--render-test-output-dir=%s' %
                          render_tests_device_output_dir)

    if flags_to_add:
      self._CreateFlagChangerIfNeeded(device)
      self._flag_changers[str(device)].PushFlags(add=flags_to_add)

    time_ms = lambda: int(time.time() * 1e3)
    start_ms = time_ms()

    stream_name = 'logcat_%s_%s_%s' % (
        test_name.replace('#', '.'),
        time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()),
        device.serial)
    logmon = logdog_logcat_monitor.LogdogLogcatMonitor(
        device.adb, stream_name, filter_specs=LOGCAT_FILTERS)

    with contextlib_ext.Optional(
        logmon, self._test_instance.should_save_logcat):
      with _LogTestEndpoints(device, test_name):
        with contextlib_ext.Optional(
            trace_event.trace(test_name),
            self._env.trace_output):
          output = device.StartInstrumentation(
              target, raw=True, extras=extras, timeout=timeout, retries=0)

    logcat_url = logmon.GetLogcatURL()
    duration_ms = time_ms() - start_ms

    if self._env.trace_output:
      self._SaveTraceData(trace_device_file, device, test['class'])

    # TODO(jbudorick): Make instrumentation tests output a JSON so this
    # doesn't have to parse the output.
    result_code, result_bundle, statuses = (
        self._test_instance.ParseAmInstrumentRawOutput(output))
    results = self._test_instance.GenerateTestResults(
        result_code, result_bundle, statuses, start_ms, duration_ms,
        device.product_cpu_abi, self._test_instance.symbolizer)

    def restore_flags():
      if flags_to_add:
        self._flag_changers[str(device)].Restore()

    def restore_timeout_scale():
      if test_timeout_scale:
        valgrind_tools.SetChromeTimeoutScale(
            device, self._test_instance.timeout_scale)

    def handle_coverage_data():
      if self._test_instance.coverage_directory:
        device.PullFile(coverage_directory,
            self._test_instance.coverage_directory)
        device.RunShellCommand(
            'rm -f %s' % posixpath.join(coverage_directory, '*'),
            check_return=True, shell=True)

    def handle_render_test_data():
      if _IsRenderTest(test):
        # Render tests do not cause test failure by default. So we have to check
        # to see if any failure images were generated even if the test does not
        # fail.
        try:
          self._ProcessRenderTestResults(
              device, render_tests_device_output_dir, results)
        finally:
          device.RemovePath(render_tests_device_output_dir,
                            recursive=True, force=True)

    # While constructing the TestResult objects, we can parallelize several
    # steps that involve ADB. These steps should NOT depend on any info in
    # the results! Things such as whether the test CRASHED have not yet been
    # determined.
    post_test_steps = [restore_flags, restore_timeout_scale,
                       handle_coverage_data, handle_render_test_data]
    if self._env.concurrent_adb:
      post_test_step_thread_group = reraiser_thread.ReraiserThreadGroup(
          reraiser_thread.ReraiserThread(f) for f in post_test_steps)
      post_test_step_thread_group.StartAll(will_block=True)
    else:
      for step in post_test_steps:
        step()

    for result in results:
      if logcat_url:
        result.SetLink('logcat', logcat_url)

    # Update the result name if the test used flags.
    if flags_to_add:
      for r in results:
        if r.GetName() == test_name:
          r.SetName(test_display_name)

    # Add UNKNOWN results for any missing tests.
    iterable_test = test if isinstance(test, list) else [test]
    test_names = set(self._GetUniqueTestName(t) for t in iterable_test)
    results_names = set(r.GetName() for r in results)
    results.extend(
        base_test_result.BaseTestResult(u, base_test_result.ResultType.UNKNOWN)
        for u in test_names.difference(results_names))

    # Update the result type if we detect a crash.
    if DidPackageCrashOnDevice(self._test_instance.test_package, device):
      for r in results:
        if r.GetType() == base_test_result.ResultType.UNKNOWN:
          r.SetType(base_test_result.ResultType.CRASH)

    # Handle failures by:
    #   - optionally taking a screenshot
    #   - logging the raw output at INFO level
    #   - clearing the application state while persisting permissions
    if any(r.GetType() not in (base_test_result.ResultType.PASS,
                               base_test_result.ResultType.SKIP)
           for r in results):
      with contextlib_ext.Optional(
          tempfile_ext.NamedTemporaryDirectory(),
          self._test_instance.screenshot_dir is None and
              self._test_instance.gs_results_bucket) as screenshot_host_dir:
        screenshot_host_dir = (
            self._test_instance.screenshot_dir or screenshot_host_dir)
        self._SaveScreenshot(device, screenshot_host_dir,
                             screenshot_device_file, test_display_name,
                             results)

      logging.info('detected failure in %s. raw output:', test_display_name)
      for l in output:
        logging.info('  %s', l)
      if (not self._env.skip_clear_data
          and self._test_instance.package_info):
        permissions = (
            self._test_instance.apk_under_test.GetPermissions()
            if self._test_instance.apk_under_test
            else None)
        device.ClearApplicationState(self._test_instance.package_info.package,
                                     permissions=permissions)
    else:
      logging.debug('raw output from %s:', test_display_name)
      for l in output:
        logging.debug('  %s', l)
    if self._test_instance.store_tombstones:
      tombstones_url = None
      for result in results:
        if result.GetType() == base_test_result.ResultType.CRASH:
          if not tombstones_url:
            resolved_tombstones = tombstones.ResolveTombstones(
                device,
                resolve_all_tombstones=True,
                include_stack_symbols=False,
                wipe_tombstones=True,
                tombstone_symbolizer=self._test_instance.symbolizer)
            stream_name = 'tombstones_%s_%s' % (
                time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime()),
                device.serial)
            tombstones_url = logdog_helper.text(
                stream_name, '\n'.join(resolved_tombstones))
          result.SetLink('tombstones', tombstones_url)

    if self._env.concurrent_adb:
      post_test_step_thread_group.JoinAll()
    return results, None

  def _GetTestsFromRunner(self):
    test_apk_path = self._test_instance.test_apk.path
    pickle_path = '%s-runner.pickle' % test_apk_path
    try:
      return instrumentation_test_instance.GetTestsFromPickle(
          pickle_path, test_apk_path)
    except instrumentation_test_instance.TestListPickleException as e:
      logging.info('Could not get tests from pickle: %s', e)
    logging.info('Getting tests by having %s list them.',
                 self._test_instance.junit4_runner_class)
    def list_tests(dev):
      with device_temp_file.DeviceTempFile(
          dev.adb, suffix='.json',
          dir=dev.GetExternalStoragePath()) as dev_test_list_json:
        junit4_runner_class = self._test_instance.junit4_runner_class
        test_package = self._test_instance.test_package
        extras = {}
        extras['log'] = 'true'
        extras[_EXTRA_TEST_LIST] = dev_test_list_json.name
        target = '%s/%s' % (test_package, junit4_runner_class)
        test_list_run_output = dev.StartInstrumentation(
            target, extras=extras)
        if any(test_list_run_output):
          logging.error('Unexpected output while listing tests:')
          for line in test_list_run_output:
            logging.error('  %s', line)
        with tempfile_ext.NamedTemporaryDirectory() as host_dir:
          host_file = os.path.join(host_dir, 'list_tests.json')
          dev.PullFile(dev_test_list_json.name, host_file)
          with open(host_file, 'r') as host_file:
              return json.load(host_file)

    raw_test_lists = self._env.parallel_devices.pMap(list_tests).pGet(None)

    # If all devices failed to list tests, raise an exception.
    # Check that tl is not None and is not empty.
    if all(not tl for tl in raw_test_lists):
      raise device_errors.CommandFailedError(
          'Failed to list tests on any device')

    # Get the first viable list of raw tests
    raw_tests = [tl for tl in raw_test_lists if tl][0]

    instrumentation_test_instance.SaveTestsToPickle(
        pickle_path, test_apk_path, raw_tests)
    return raw_tests

  def _SaveTraceData(self, trace_device_file, device, test_class):
    trace_host_file = self._env.trace_output

    if device.FileExists(trace_device_file.name):
      try:
        java_trace_json = device.ReadFile(trace_device_file.name)
      except IOError:
        raise Exception('error pulling trace file from device')
      finally:
        trace_device_file.close()

      process_name = '%s (device %s)' % (test_class, device.serial)
      process_hash = int(hashlib.md5(process_name).hexdigest()[:6], 16)

      java_trace = json.loads(java_trace_json)
      java_trace.sort(key=lambda event: event['ts'])

      get_date_command = 'echo $EPOCHREALTIME'
      device_time = device.RunShellCommand(get_date_command, single_line=True)
      device_time = float(device_time) * 1e6
      system_time = trace_time.Now()
      time_difference = system_time - device_time

      threads_to_add = set()
      for event in java_trace:
        # Ensure thread ID and thread name will be linked in the metadata.
        threads_to_add.add((event['tid'], event['name']))

        event['pid'] = process_hash

        # Adjust time stamp to align with Python trace times (from
        # trace_time.Now()).
        event['ts'] += time_difference

      for tid, thread_name in threads_to_add:
        thread_name_metadata = {'pid': process_hash, 'tid': tid,
                                'ts': 0, 'ph': 'M', 'cat': '__metadata',
                                'name': 'thread_name',
                                'args': {'name': thread_name}}
        java_trace.append(thread_name_metadata)

      process_name_metadata = {'pid': process_hash, 'tid': 0, 'ts': 0,
                               'ph': 'M', 'cat': '__metadata',
                               'name': 'process_name',
                               'args': {'name': process_name}}
      java_trace.append(process_name_metadata)

      java_trace_json = json.dumps(java_trace)
      java_trace_json = java_trace_json.rstrip(' ]')

      with open(trace_host_file, 'r') as host_handle:
        host_contents = host_handle.readline()

      if host_contents:
        java_trace_json = ',%s' % java_trace_json.lstrip(' [')

      with open(trace_host_file, 'a') as host_handle:
        host_handle.write(java_trace_json)

  def _SaveScreenshot(self, device, screenshot_host_dir, screenshot_device_file,
                      test_name, results):
    if screenshot_host_dir:
      screenshot_host_file = os.path.join(
          screenshot_host_dir,
          '%s-%s.png' % (
              test_name,
              time.strftime('%Y%m%dT%H%M%S-UTC', time.gmtime())))
      if device.FileExists(screenshot_device_file.name):
        try:
          device.PullFile(screenshot_device_file.name, screenshot_host_file)
        finally:
          screenshot_device_file.close()

        logging.info(
            'Saved screenshot for %s to %s.',
            test_name, screenshot_host_file)
        if self._test_instance.gs_results_bucket:
          link = google_storage_helper.upload(
              google_storage_helper.unique_name(
                  'screenshot', device=device),
              screenshot_host_file,
              bucket=('%s/screenshots' %
                      self._test_instance.gs_results_bucket))
          for result in results:
            result.SetLink('post_test_screenshot', link)

  def _ProcessRenderTestResults(
      self, device, render_tests_device_output_dir, results):
    # If GS results bucket is specified, will archive render result images.
    # If render image dir is specified, will pull the render result image from
    # the device and leave in the directory.
    if not (bool(self._test_instance.gs_results_bucket) or
            bool(self._test_instance.render_results_dir)):
      return

    failure_images_device_dir = posixpath.join(
        render_tests_device_output_dir, 'failures')
    if not device.FileExists(failure_images_device_dir):
      return

    diff_images_device_dir = posixpath.join(
        render_tests_device_output_dir, 'diffs')

    golden_images_device_dir = posixpath.join(
        render_tests_device_output_dir, 'goldens')

    with contextlib_ext.Optional(
        tempfile_ext.NamedTemporaryDirectory(),
        not bool(self._test_instance.render_results_dir)) as render_temp_dir:
      render_host_dir = (
          self._test_instance.render_results_dir or render_temp_dir)

      if not os.path.exists(render_host_dir):
        os.makedirs(render_host_dir)

      # Pull all render test results from device.
      device.PullFile(failure_images_device_dir, render_host_dir)

      if device.FileExists(diff_images_device_dir):
        device.PullFile(diff_images_device_dir, render_host_dir)
      else:
        logging.error('Diff images not found on device.')

      if device.FileExists(golden_images_device_dir):
        device.PullFile(golden_images_device_dir, render_host_dir)
      else:
        logging.error('Golden images not found on device.')

      # Upload results to Google Storage.
      if self._test_instance.gs_results_bucket:
        self._UploadRenderTestResults(render_host_dir, results)

  def _UploadRenderTestResults(self, render_host_dir, results):
    render_tests_bucket = (
        self._test_instance.gs_results_bucket + '/render_tests')

    for failure_filename in os.listdir(
        os.path.join(render_host_dir, 'failures')):
      m = RE_RENDER_IMAGE_NAME.match(failure_filename)
      if not m:
        logging.warning('Unexpected file in render test failures: %s',
                        failure_filename)
        continue

      failure_filepath = os.path.join(
          render_host_dir, 'failures', failure_filename)
      failure_link = google_storage_helper.upload_content_addressed(
          failure_filepath, bucket=render_tests_bucket)

      golden_filepath = os.path.join(
          render_host_dir, 'goldens', failure_filename)
      if os.path.exists(golden_filepath):
        golden_link = google_storage_helper.upload_content_addressed(
            golden_filepath, bucket=render_tests_bucket)
      else:
        golden_link = ''

      diff_filepath = os.path.join(
          render_host_dir, 'diffs', failure_filename)
      if os.path.exists(diff_filepath):
        diff_link = google_storage_helper.upload_content_addressed(
            diff_filepath, bucket=render_tests_bucket)
      else:
        diff_link = ''

      with tempfile.NamedTemporaryFile(suffix='.html') as temp_html:
        jinja2_env = jinja2.Environment(
            loader=jinja2.FileSystemLoader(_JINJA_TEMPLATE_DIR),
            trim_blocks=True)
        template = jinja2_env.get_template(_JINJA_TEMPLATE_FILENAME)
        # pylint: disable=no-member
        processed_template_output = template.render(
            test_name=failure_filename,
            failure_link=failure_link,
            golden_link=golden_link,
            diff_link=diff_link)

        temp_html.write(processed_template_output)
        temp_html.flush()
        html_results_link = google_storage_helper.upload_content_addressed(
            temp_html.name,
            bucket=render_tests_bucket,
            content_type='text/html')
        for result in results:
          result.SetLink(failure_filename, html_results_link)

  #override
  def _ShouldRetry(self, test, result):
    def not_run(res):
      if isinstance(res, list):
        return any(not_run(r) for r in res)
      return res.GetType() == base_test_result.ResultType.NOTRUN

    if 'RetryOnFailure' in test.get('annotations', {}) or not_run(result):
      return True

    # TODO(jbudorick): Remove this log message once @RetryOnFailure has been
    # enabled for a while. See crbug.com/619055 for more details.
    logging.error('Default retries are being phased out. crbug.com/619055')
    return False

  #override
  def _ShouldShard(self):
    return True

  @classmethod
  def _GetTimeoutScaleFromAnnotations(cls, annotations):
    try:
      return int(annotations.get('TimeoutScale', {}).get('value', 1))
    except ValueError as e:
      logging.warning("Non-integer value of TimeoutScale ignored. (%s)", str(e))
      return 1

  @classmethod
  def _GetTimeoutFromAnnotations(cls, annotations, test_name):
    for k, v in TIMEOUT_ANNOTATIONS:
      if k in annotations:
        timeout = v
        break
    else:
      logging.warning('Using default 1 minute timeout for %s', test_name)
      timeout = 60

    timeout *= cls._GetTimeoutScaleFromAnnotations(annotations)

    return timeout

def _IsRenderTest(test):
  """Determines if a test or list of tests has a RenderTest amongst them."""
  if not isinstance(test, list):
    test = [test]
  return any([RENDER_TEST_FEATURE_ANNOTATION in t['annotations'].get(
              FEATURE_ANNOTATION, {}).get('value', ()) for t in test])
