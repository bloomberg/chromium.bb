# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import io
import json
import logging
import os
import pickle
import shutil
import tempfile
import time
import zipfile

from devil.android import battery_utils
from devil.android import device_blacklist
from devil.android import device_errors
from devil.android import device_list
from devil.android import device_utils
from devil.android import forwarder
from devil.android.tools import device_recovery
from devil.android.tools import device_status
from devil.utils import cmd_helper
from devil.utils import parallelizer
from pylib import constants
from pylib.base import base_test_result
from pylib.constants import host_paths
from pylib.local.device import local_device_test_run


class TestShard(object):
  def __init__(
      self, env, test_instance, device, index, tests, retries=3, timeout=None):
    logging.info('Create shard %s for device %s to run the following tests:',
                 index, device)
    for t in tests:
      logging.info('  %s', t)
    self._battery = battery_utils.BatteryUtils(device)
    self._device = device
    self._env = env
    self._index = index
    self._output_dir = None
    self._retries = retries
    self._test_instance = test_instance
    self._tests = tests
    self._timeout = timeout

  @local_device_test_run.handle_shard_failures
  def RunTestsOnShard(self):
    results = base_test_result.TestRunResults()
    for test in self._tests:
      tries_left = self._retries
      result_type = None
      while (result_type != base_test_result.ResultType.PASS
             and tries_left > 0):
        try:
          self._TestSetUp(test)
          result_type = self._RunSingleTest(test)
        except device_errors.CommandTimeoutError:
          result_type = base_test_result.ResultType.TIMEOUT
        except device_errors.CommandFailedError:
          logging.exception('Exception when executing %s.', test)
          result_type = base_test_result.ResultType.FAIL
        finally:
          self._TestTearDown()
          if result_type != base_test_result.ResultType.PASS:
            try:
              device_recovery.RecoverDevice(self._device, self._env.blacklist)
            except device_errors.CommandTimeoutError:
              logging.exception(
                  'Device failed to recover after failing %s.', test)
          tries_left = tries_left - 1

      results.AddResult(base_test_result.BaseTestResult(test, result_type))
    return results

  def _TestSetUp(self, test):
    if not self._device.IsOnline():
      msg = 'Device %s is unresponsive.' % str(self._device)
      raise device_errors.DeviceUnreachableError(msg)

    logging.info('Charge level: %s%%',
                 str(self._battery.GetBatteryInfo().get('level')))
    if self._test_instance.min_battery_level:
      self._battery.ChargeDeviceToLevel(self._test_instance.min_battery_level)

    logging.info('temperature: %s (0.1 C)',
                 str(self._battery.GetBatteryInfo().get('temperature')))
    if self._test_instance.max_battery_temp:
      self._battery.LetBatteryCoolToTemperature(
          self._test_instance.max_battery_temp)

    if not self._device.IsScreenOn():
      self._device.SetScreen(True)

    if (self._test_instance.collect_chartjson_data
        or self._tests[test].get('archive_output_dir')):
      self._output_dir = tempfile.mkdtemp()

  def _RunSingleTest(self, test):
    self._test_instance.WriteBuildBotJson(self._output_dir)

    timeout = self._tests[test].get('timeout', self._timeout)
    cmd = self._CreateCmd(test)
    cwd = os.path.abspath(host_paths.DIR_SOURCE_ROOT)

    logging.debug("Running %s with command '%s' on shard %d with timeout %d",
                  test, cmd, self._index, timeout)

    try:
      start_time = time.time()
      exit_code, output = cmd_helper.GetCmdStatusAndOutputWithTimeout(
          cmd, timeout, cwd=cwd, shell=True)
      end_time = time.time()
      json_output = self._test_instance.ReadChartjsonOutput(self._output_dir)
      if exit_code == 0:
        result_type = base_test_result.ResultType.PASS
      else:
        result_type = base_test_result.ResultType.FAIL
    except cmd_helper.TimeoutError as e:
      end_time = time.time()
      exit_code = -1
      output = e.output
      json_output = ''
      result_type = base_test_result.ResultType.TIMEOUT

    return self._ProcessTestResult(test, cmd, start_time, end_time, exit_code,
                                   output, json_output, result_type)

  def _CreateCmd(self, test):
    cmd = '%s --device %s' % (self._tests[test]['cmd'], str(self._device))
    if self._output_dir:
      cmd = cmd + ' --output-dir=%s' % self._output_dir
    if self._test_instance.dry_run:
      cmd = 'echo %s' % cmd
    return cmd

  def _ProcessTestResult(self, test, cmd, start_time, end_time, exit_code,
                         output, json_output, result_type):
    if exit_code is None:
      exit_code = -1
    logging.info('%s : exit_code=%d in %d secs on device %s',
                 test, exit_code, end_time - start_time,
                 str(self._device))

    actual_exit_code = exit_code
    if (self._test_instance.flaky_steps
        and test in self._test_instance.flaky_steps):
      exit_code = 0
    archive_bytes = (self._ArchiveOutputDir()
                     if self._tests[test].get('archive_output_dir')
                     else None)
    persisted_result = {
        'name': test,
        'output': [output],
        'chartjson': json_output,
        'archive_bytes': archive_bytes,
        'exit_code': exit_code,
        'actual_exit_code': actual_exit_code,
        'result_type': result_type,
        'start_time': start_time,
        'end_time': end_time,
        'total_time': end_time - start_time,
        'device': str(self._device),
        'cmd': cmd,
    }
    self._SaveResult(persisted_result)
    return result_type

  def _ArchiveOutputDir(self):
    """Archive all files in the output dir, and return as compressed bytes."""
    with io.BytesIO() as archive:
      with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as contents:
        num_files = 0
        for absdir, _, files in os.walk(self._output_dir):
          reldir = os.path.relpath(absdir, self._output_dir)
          for filename in files:
            src_path = os.path.join(absdir, filename)
            # We use normpath to turn './file.txt' into just 'file.txt'.
            dst_path = os.path.normpath(os.path.join(reldir, filename))
            contents.write(src_path, dst_path)
            num_files += 1
      if num_files:
        logging.info('%d files in the output dir were archived.', num_files)
      else:
        logging.warning('No files in the output dir. Archive is empty.')
      return archive.getvalue()

  @staticmethod
  def _SaveResult(result):
    pickled = os.path.join(constants.PERF_OUTPUT_DIR, result['name'])
    if os.path.exists(pickled):
      with file(pickled, 'r') as f:
        previous = pickle.loads(f.read())
        result['output'] = previous['output'] + result['output']
    with file(pickled, 'w') as f:
      f.write(pickle.dumps(result))

  def _TestTearDown(self):
    if self._output_dir:
      shutil.rmtree(self._output_dir, ignore_errors=True)
      self._output_dir = None
    try:
      logging.info('Unmapping device ports for %s.', self._device)
      forwarder.Forwarder.UnmapAllDevicePorts(self._device)
    except Exception: # pylint: disable=broad-except
      logging.exception('Exception when resetting ports.')


class LocalDevicePerfTestRun(local_device_test_run.LocalDeviceTestRun):

  _DEFAULT_TIMEOUT = 60 * 60
  _CONFIG_VERSION = 1

  def __init__(self, env, test_instance):
    super(LocalDevicePerfTestRun, self).__init__(env, test_instance)
    self._devices = None
    self._env = env
    self._test_buckets = []
    self._test_instance = test_instance
    self._timeout = None if test_instance.no_timeout else self._DEFAULT_TIMEOUT

  def SetUp(self):
    self._devices = self._GetAllDevices(self._env.devices,
                                        self._test_instance.known_devices_file)

    if os.path.exists(constants.PERF_OUTPUT_DIR):
      shutil.rmtree(constants.PERF_OUTPUT_DIR)
    os.makedirs(constants.PERF_OUTPUT_DIR)

  def TearDown(self):
    pass

  def _GetStepsFromDict(self):
    # From where this is called one of these two must be set.
    if self._test_instance.single_step:
      return {
          'version': self._CONFIG_VERSION,
          'steps': {
              'single_step': {
                'device_affinity': 0,
                'cmd': self._test_instance.single_step
              },
          }
      }
    if self._test_instance.steps:
      with file(self._test_instance.steps, 'r') as f:
        steps = json.load(f)
        if steps['version'] != self._CONFIG_VERSION:
          raise TestDictVersionError(
              'Version is expected to be %d but was %d' % (self._CONFIG_VERSION,
                                                           steps['version']))
        return steps
    raise PerfTestRunGetStepsError(
        'Neither single_step or steps set in test_instance.')

  def _SplitTestsByAffinity(self):
    # This splits tests by their device affinity so that the same tests always
    # run on the same devices. This is important for perf tests since different
    # devices might yield slightly different performance results.
    test_dict = self._GetStepsFromDict()
    for test, test_config in test_dict['steps'].iteritems():
      try:
        affinity = test_config['device_affinity']
        if len(self._test_buckets) < affinity + 1:
          while len(self._test_buckets) != affinity + 1:
            self._test_buckets.append({})
        self._test_buckets[affinity][test] = test_config
      except KeyError:
        logging.exception(
            'Test config for %s is bad.\n Config:%s', test, str(test_config))

  @staticmethod
  def _GetAllDevices(active_devices, devices_path):
    try:
      if devices_path:
        devices = [device_utils.DeviceUtils(s)
                   for s in device_list.GetPersistentDeviceList(devices_path)]
        if not devices and active_devices:
          logging.warning('%s is empty. Falling back to active devices.',
                          devices_path)
          devices = active_devices
      else:
        logging.warning('Known devices file path not being passed. For device '
                        'affinity to work properly, it must be passed.')
        devices = active_devices
    except IOError as e:
      logging.error('Unable to find %s [%s]', devices_path, e)
      devices = active_devices
    return sorted(devices)

  #override
  def RunTests(self):
    # Affinitize the tests.
    self._SplitTestsByAffinity()
    if not self._test_buckets:
      raise local_device_test_run.NoTestsError()

    blacklist = (device_blacklist.Blacklist(self._env.blacklist)
                 if self._env.blacklist
                 else None)

    def run_perf_tests(shard_id):
      if device_status.IsBlacklisted(str(self._devices[shard_id]), blacklist):
        logging.warning('Device %s is not active. Will not create shard %s.',
                        str(self._devices[shard_id]), shard_id)
        return []
      s = TestShard(self._env, self._test_instance, self._devices[shard_id],
                    shard_id, self._test_buckets[shard_id],
                    retries=self._env.max_tries, timeout=self._timeout)
      return s.RunTestsOnShard()

    device_indices = range(min(len(self._devices), len(self._test_buckets)))
    shards = parallelizer.Parallelizer(device_indices).pMap(run_perf_tests)
    return shards.pGet(self._timeout)

  # override
  def TestPackage(self):
    return 'perf'

  # override
  def _CreateShards(self, _tests):
    raise NotImplementedError

  # override
  def _GetTests(self):
    return self._test_buckets

  # override
  def _RunTest(self, _device, _test):
    raise NotImplementedError

  # override
  def _ShouldShard(self):
    return False


class OutputJsonList(LocalDevicePerfTestRun):
  # override
  def SetUp(self):
    pass

  # override
  def RunTests(self):
    result_type = self._test_instance.OutputJsonList()
    result = base_test_result.TestRunResults()
    result.AddResult(
        base_test_result.BaseTestResult('OutputJsonList', result_type))
    return [result]

  # override
  def _CreateShards(self, _tests):
    raise NotImplementedError

  # override
  def _RunTest(self, _device, _test):
    raise NotImplementedError


class PrintStep(LocalDevicePerfTestRun):
  # override
  def SetUp(self):
    pass

  # override
  def RunTests(self):
    result_type = self._test_instance.PrintTestOutput()
    result = base_test_result.TestRunResults()
    result.AddResult(
        base_test_result.BaseTestResult('PrintStep', result_type))
    return [result]

  # override
  def _CreateShards(self, _tests):
    raise NotImplementedError

  # override
  def _RunTest(self, _device, _test):
    raise NotImplementedError


class TestDictVersionError(Exception):
  pass

class PerfTestRunGetStepsError(Exception):
  pass
