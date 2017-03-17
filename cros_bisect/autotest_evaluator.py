# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs autotest on DUT and gets result for performance evaluation."""

from __future__ import print_function

import os
import shutil

from chromite.cros_bisect import common
from chromite.cros_bisect import evaluator
from chromite.cros_bisect import simple_chrome_builder
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import json_lib
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import remote_access


class AutotestEvaluator(evaluator.Evaluator):
  """Evaluates performance by running autotest test.

  It first try running autotest from DUT (via ssh command). If it fails to run
  (e.g. first running the test on the DUT), it then runs "test_that" inside
  CrOS's chrome_sdk to pack test package and push it to DUT to execute.

  After autotest done running, it grabs JSON report file and finds the metric
  to watch (currently support float value).

  If reuse_eval is set, before running autotest, it checks if the previous
  report exists. If so, just extracts metric value from it. It saves time
  for continuing an interrupted bisect.
  """
  AUTOTEST_BASE = '/usr/local/autotest'
  AUTOTEST_CLIENT = os.path.join(AUTOTEST_BASE, 'bin/autotest_client')
  REQUIRED_ARGS = evaluator.Evaluator.REQUIRED_ARGS + (
      'board', 'test_name', 'metric', 'metric_take_average', 'reuse_eval')

  def __init__(self, options):
    """Constructor.

    Args:
      options: In addition to the flags required by the base class, need to
        specify:
        * board: CrOS board name (used for running host side autotest).
        * chromium_dir: Optional. If specified, use the chromium repo the path
          points to. Otherwise, use base_dir/chromium/src.
        * test_name: Autotest name to run.
        * metric: Metric to look up.
        * metric_take_average: If set, take average value of the metric.
        * reuse_eval: True to reuse report if available.
    """
    super(AutotestEvaluator, self).__init__(options)
    self.board = options.board
    self.test_name = options.test_name
    self.metric = options.metric
    self.metric_take_average = options.metric_take_average
    self.reuse_eval = options.reuse_eval
    # Used for entering chroot. Some autotest depends on CHROME_ROOT being set.
    if 'chromium_dir' in options and options.chromium_dir:
      self.chromium_dir = options.chromium_dir
    else:
      self.chromium_dir = os.path.join(self.base_dir,
                                       simple_chrome_builder.CHROMIUM_DIR)

  def RunTestFromDut(self, remote, report_file):
    """Runs autotest from DUT.

    It runs autotest from DUT directly. It can only be used after the test was
    deployed/run using "test_that" from host.

    Args:
      remote: DUT for running test (refer lib.commandline.Device).
      report_file: Benchmark report to store (host side).

    Returns:
      True if autotest ran successfully.
    """
    # TODO(deanliao): Deal with the case that test control file is not the
    #     same as below.
    test_target = '%s/tests/%s/control' % (self.AUTOTEST_BASE, self.test_name)
    remote_report_file = '%s/results/default/%s/results/results-chart.json' % (
        self.AUTOTEST_BASE, self.test_name)

    with osutils.TempDir() as temp_dir:
      command = [self.AUTOTEST_CLIENT, test_target]
      logging.info('Run autotest from DUT %s: %s', remote.raw,
                   cros_build_lib.CmdToStr(command))
      dut = remote_access.RemoteAccess(
          remote.hostname, temp_dir, port=remote.port, username=remote.username)
      command_result = dut.RemoteSh(command, error_code_ok=True)
      if command_result.returncode == 0:
        logging.info('Ran successfully. Copy report from %s:%s to %s',
                     remote.raw, remote_report_file, report_file)
        command_result = dut.ScpToLocal(remote_report_file, report_file)
        if command_result.returncode != 0:
          logging.info('Failed to copy report from DUT(%s:%s) to host(%s)',
                       remote.raw, remote_report_file, report_file)
      else:
        logging.info('Failed to run autotest from DUT. returncode: %d',
                     command_result.returncode)

    return command_result.returncode == 0

  def LookupReportFile(self):
    """Looks up autotest report file.

    It looks up results-chart.json under chroot's /tmp/test_that_latest.

    Returns:
      Path to report file. None if not found.
    """
    RESULT_FILENAME = 'results-chart.json'

    # Default result dir: /tmp/test_that_latest
    results_dir = path_util.FromChrootPath(
        '/tmp/test_that_latest/results-1-%s' % self.test_name)
    # Invoking "find" command is faster than using os.walkdir().
    try:
      command_result = cros_build_lib.RunCommand(
          ['find', '.', '-name', RESULT_FILENAME],
          cwd=results_dir, capture_output=True)
    except cros_build_lib.RunCommandError as e:
      logging.error('Failed to look up %s under %s: %s', RESULT_FILENAME,
                    results_dir, e)
      return None
    if not command_result.output:
      logging.error('Failed to look up %s under %s', RESULT_FILENAME,
                    results_dir)
      return None
    report_file_under_results_dir = (
        command_result.output.splitlines()[0].strip())
    return os.path.normpath(
        os.path.join(results_dir, report_file_under_results_dir))

  def RunTestFromHost(self, remote, report_file_to_store):
    """Runs autotest from host.

    It uses test_that tool in CrOS chroot to deploy autotest to DUT and run it.

    Args:
      remote: DUT for running test (refer lib.commandline.Device).
      report_file_to_store: Benchmark report to store.

    Returns:
      True if autotest ran successfully.
    """
    run_autotest = ['test_that',
                    '-b', self.board,
                    '--fast',
                    '--args', 'local=True',
                    remote.raw,
                    self.test_name]
    # --chrome_root is needed for autotests running Telemetry.
    # --no-ns-pid is used to prevent the program receiving SIGTTIN (e.g. go to
    # background and stopped) when asking user input.
    chroot_args = ['--chrome_root', self.chromium_dir,
                   '--no-ns-pid']

    # TODO(deanliao): Can specify different cros repo.
    try:
      cros_build_lib.RunCommand(run_autotest, enter_chroot=True,
                                chroot_args=chroot_args)
    except cros_build_lib.RunCommandError as e:
      logging.error('Failed to run autotest: %s', e)
      return False

    report_file_in_chroot = self.LookupReportFile()
    if not report_file_in_chroot:
      logging.error('Failed to run autotest: report file not found')
      return False

    shutil.copyfile(report_file_in_chroot, report_file_to_store)
    return True

  def GetAutotestMetricValue(self, report_file):
    """Gets metric value from autotest benchmark report.

    Report example:
      {"avg_fps_1000_fishes": {
         "summary": {
           "units": "fps",
           "type": "scalar",
           "value": 56.733810392225671,
           "improvement_direction": "up"
         },
         ...,
       },
       ...,
      }
      self.metric = "avg_fps_1000_fishes/summary/value"

    Args:
      report_file: Path to benchmark report.

    Returns:
      Metric value in benchmark report.
      None if self.metric is undefined or metric does not exist in the report.
    """
    if not self.metric:
      return None

    report = json_lib.ParseJsonFileWithComments(report_file)
    metric_value = json_lib.GetNestedDictValue(report, self.metric.split('/'))
    if metric_value is None:
      logging.error('Cannot get metric %s from %s', self.metric, report_file)
      return None
    if self.metric_take_average:
      return float(sum(metric_value)) / len(metric_value)
    return metric_value

  def Evaluate(self, remote, build_label, repeat=1):
    """Runs autotest N-times on DUT and extracts the designated metric values.

    Args:
      remote: DUT to evaluate (refer lib.commandline.Device).
      build_label: Build label used for part of report filename and log message.
      repeat: Run test for N times. Default 1.

    Returns:
      Score object stores a list of autotest running results.
    """
    if repeat == 1:
      times_str = 'once'
    elif repeat == 2:
      times_str = 'twice'
    else:
      times_str = '%d times' % repeat
    logging.info(
        'Evaluating build %s performance on DUT %s by running autotest %s %s '
        'to get metric %s',
        build_label, remote.raw, self.test_name, times_str, self.metric)

    score_list = []
    for nth in range(repeat):
      report_file = os.path.join(
          self.report_base_dir,
          'results-chart.%s.%d-%d.json' % (build_label, nth + 1, repeat))
      score = self._EvaluateOnce(remote, report_file)
      if score is None:
        return common.Score()
      logging.info(
          'Run autotest %d/%d. Got result: %s:%s = %.3f (build:%s DUT:%s).',
          nth + 1, repeat, self.test_name, self.metric, score, build_label,
          remote.raw)
      score_list.append(score)

    scores = common.Score(score_list)
    logging.info(
        'Successfully ran autotest %d times. Arithmetic mean(%s:%s) = %.3f',
        repeat, self.test_name, self.metric, scores.mean)
    return scores

  def _EvaluateOnce(self, remote, report_file):
    """Runs autotest on DUT once and extracts the designated metric value."""
    success = self.RunTestFromDut(remote, report_file)
    if not success:
      logging.info('Failed to run autotest from DUT. Failover to run autotest '
                   'from host using "test_that" command.')
      success = self.RunTestFromHost(remote, report_file)
      if not success:
        logging.error('Failed to run autotest.')
        return None
    return self.GetAutotestMetricValue(report_file)

  def CheckLastEvaluate(self, build_label, repeat=1):
    """Checks if previous evaluate report is available.

    Args:
      build_label: Build label used for part of report filename and log message.
      repeat: Run test for N times. Default 1.

    Returns:
      Score object stores a list of autotest running results if report
      available and reuse_eval is set.
      Score() otherwise.
    """
    if not self.reuse_eval:
      return common.Score()

    score_list = []
    for nth in range(repeat):
      report_file = os.path.join(
          self.report_base_dir,
          'results-chart.%s.%d-%d.json' % (build_label, nth + 1, repeat))
      if not os.path.isfile(report_file):
        return common.Score()
      score = self.GetAutotestMetricValue(report_file)
      if score is None:
        return common.Score()
      score_list.append(score)
    scores = common.Score(score_list)
    logging.info(
        'Used archived autotest result. Arithmetic mean(%s:%s) = '
        '%.3f (build:%s)',
        self.test_name, self.metric, scores.mean, build_label)
    return scores
