# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the test stages."""

import logging
import os

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_failures as failures_lib
from chromite.buildbot import constants
from chromite.buildbot import lab_status
from chromite.buildbot import validation_pool
from chromite.buildbot.stages import generic_stages
from chromite.lib import cros_build_lib
from chromite.lib import osutils
from chromite.lib import timeout_util


_VM_TEST_ERROR_MSG = """
!!!VMTests failed!!!

Logs are uploaded in the corresponding %(vm_test_results)s. This can be found
by clicking on the artifacts link in the "Report" Stage. Specifically look
for the test_harness/failed for the failing tests. For more
particulars, please refer to which test failed i.e. above see the
individual test that failed -- or if an update failed, check the
corresponding update directory.
"""
PRE_CQ = validation_pool.PRE_CQ

CQ_HWTEST_WAS_ABORTED = ('HWTest was aborted, because another commit '
                         'queue builder failed outside of HWTest.')


class UnitTestStage(generic_stages.BoardSpecificBuilderStage):
  """Run unit tests."""

  option_name = 'tests'
  config_name = 'unittests'

  # If the unit tests take longer than 70 minutes, abort. They usually take
  # ten minutes to run.
  #
  # If the processes hang, parallel_emerge will print a status report after 60
  # minutes, so we picked 70 minutes because it gives us a little buffer time.
  UNIT_TEST_TIMEOUT = 70 * 60

  def PerformStage(self):
    extra_env = {}
    if self._run.config.useflags:
      extra_env['USE'] = ' '.join(self._run.config.useflags)
    with timeout_util.Timeout(self.UNIT_TEST_TIMEOUT):
      commands.RunUnitTests(self._build_root,
                            self._current_board,
                            full=(not self._run.config.quick_unit),
                            blacklist=self._run.config.unittest_blacklist,
                            extra_env=extra_env)

    if os.path.exists(os.path.join(self.GetImageDirSymlink(),
                                   'au-generator.zip')):
      commands.TestAuZip(self._build_root,
                         self.GetImageDirSymlink())


class VMTestStage(generic_stages.BoardSpecificBuilderStage,
                  generic_stages.ArchivingStageMixin):
  """Run autotests in a virtual machine."""

  option_name = 'tests'
  config_name = 'vm_tests'

  VM_TEST_TIMEOUT = 60 * 60

  def _PrintFailedTests(self, results_path, test_basename):
    """Print links to failed tests.

    Args:
      results_path: Path to directory containing the test results.
      test_basename: The basename that the tests are archived to.
    """
    test_list = commands.ListFailedTests(results_path)
    for test_name, path in test_list:
      self.PrintDownloadLink(
          os.path.join(test_basename, path), text_to_display=test_name)

  def _ArchiveTestResults(self, test_results_dir, test_basename):
    """Archives test results to Google Storage.

    Args:
      test_results_dir: Name of the directory containing the test results.
      test_basename: The basename to archive the tests.
    """
    results_path = commands.GetTestResultsDir(
        self._build_root, test_results_dir)

    # Skip archiving if results_path does not exist or is an empty directory.
    if not os.path.isdir(results_path) or not os.listdir(results_path):
      return

    archived_results_dir = os.path.join(self.archive_path, test_basename)
    # Copy relevant files to archvied_results_dir.
    commands.ArchiveTestResults(results_path, archived_results_dir)
    upload_paths = [os.path.basename(archived_results_dir)]
    # Create the compressed tarball to upload.
    # TODO: We should revisit whether uploading the tarball is necessary.
    test_tarball = commands.BuildAndArchiveTestResultsTarball(
        archived_results_dir, self._build_root)
    upload_paths.append(test_tarball)

    got_symbols = self.GetParallel('breakpad_symbols_generated',
                                   pretty_name='breakpad symbols')
    upload_paths += commands.GenerateStackTraces(
        self._build_root, self._current_board, test_results_dir,
        self.archive_path, got_symbols)

    self._Upload(upload_paths)
    self._PrintFailedTests(results_path, test_basename)

    # Remove the test results directory.
    osutils.RmDir(results_path, ignore_missing=True, sudo=True)

  def _ArchiveVMFiles(self, test_results_dir):
    vm_files = commands.ArchiveVMFiles(
        self._build_root, os.path.join(test_results_dir, 'test_harness'),
        self.archive_path)
    # We use paths relative to |self.archive_path|, for prettier
    # formatting on the web page.
    self._Upload([os.path.basename(image) for image in vm_files])

  def _Upload(self, filenames):
    cros_build_lib.Info('Uploading artifacts to Google Storage...')
    with self.ArtifactUploader(archive=False, strict=False) as queue:
      for filename in filenames:
        queue.put([filename])
        if filename.endswith('.dmp.txt'):
          prefix = 'crash: '
        elif constants.VM_DISK_PREFIX in os.path.basename(filename):
          prefix = 'vm_disk: '
        elif constants.VM_MEM_PREFIX in os.path.basename(filename):
          prefix = 'vm_memory: '
        else:
          prefix = ''
        self.PrintDownloadLink(filename, prefix)

  def _RunTest(self, test_type, test_results_dir):
    """Run a VM test.

    Args:
      test_type: Any test in constants.VALID_VM_TEST_TYPES
      test_results_dir: The base directory to store the results.
    """
    if test_type == constants.CROS_VM_TEST_TYPE:
      commands.RunCrosVMTest(self._current_board, self.GetImageDirSymlink())
    elif test_type == constants.DEV_MODE_TEST_TYPE:
      commands.RunDevModeTest(
        self._build_root, self._current_board, self.GetImageDirSymlink())
    else:
      commands.RunTestSuite(self._build_root,
                            self._current_board,
                            self.GetImageDirSymlink(),
                            os.path.join(test_results_dir,
                                         'test_harness'),
                            test_type=test_type,
                            whitelist_chrome_crashes=self._chrome_rev is None,
                            archive_dir=self.bot_archive_root)

  def PerformStage(self):
    # These directories are used later to archive test artifacts.
    test_results_dir = commands.CreateTestRoot(self._build_root)
    test_basename = constants.VM_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      for test_type in self._run.config.vm_tests:
        cros_build_lib.Info('Running VM test %s.', test_type)
        with timeout_util.Timeout(self.VM_TEST_TIMEOUT):
          self._RunTest(test_type, test_results_dir)

    except Exception:
      cros_build_lib.Error(_VM_TEST_ERROR_MSG %
                           dict(vm_test_results=test_basename))
      self._ArchiveVMFiles(test_results_dir)
      raise
    finally:
      self._ArchiveTestResults(test_results_dir, test_basename)


class HWTestStage(generic_stages.BoardSpecificBuilderStage,
                  generic_stages.ArchivingStageMixin):
  """Stage that runs tests in the Autotest lab."""

  option_name = 'tests'
  config_name = 'hw_tests'

  PERF_RESULTS_EXTENSION = 'results'

  def __init__(self, builder_run, board, suite_config, **kwargs):
    super(HWTestStage, self).__init__(builder_run, board,
                                      suffix=' [%s]' % suite_config.suite,
                                      **kwargs)
    if not self._run.IsToTBuild():
      suite_config.SetBranchedValues()

    self.suite_config = suite_config
    self.wait_for_results = True

  def _CheckAborted(self):
    """Checks with GS to see if HWTest for this build's release_tag was aborted.

    We currently only support aborting HWTests for the CQ, so this method only
    returns True for paladin builders.

    Returns:
      True if HWTest have been aborted for this build's release_tag.
      False otherwise.
    """
    aborted = (cbuildbot_config.IsCQType(self._run.config.build_type) and
               commands.HaveCQHWTestsBeenAborted(self._run.GetVersion()))
    return aborted

  # Disable complaint about calling _HandleStageException.
  # pylint: disable=W0212
  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    exc_type = exc_info[0]

    # If the config says HW Tests can only warn. Only warn.
    if self._run.config.hw_tests_warn:
      return self._HandleExceptionAsWarning(exc_info)

    # Deal with timeout errors specially.
    if issubclass(exc_type, timeout_util.TimeoutError):
      return self._HandleStageTimeoutException(exc_info)

    if self.suite_config.critical:
      return super(HWTestStage, self)._HandleStageException(exc_info)

    if self._CheckAborted():
      # HWTest was aborted. This is only applicable to CQ.
      logging.warning(CQ_HWTEST_WAS_ABORTED)
      return self._HandleExceptionAsWarning(exc_info)
    elif issubclass(exc_type, commands.TestWarning):
      # HWTest passed with warning. All builders should pass.
      logging.warning('HWTest passed with warning code.')
      return self._HandleExceptionAsWarning(exc_info)
    elif issubclass(exc_type, failures_lib.InfrastructureFailure):
      # Tests did not run correctly; builders that do not check in
      # code should pass.
      logging.warning('HWTest did not complete due to infrastructure issues '
                      '(%s)', exc_type)
      if not cbuildbot_config.IsPFQType(self._run.config.build_type):
        return self._HandleExceptionAsWarning(exc_info)

    return super(HWTestStage, self)._HandleStageException(exc_info)

  def _HandleStageTimeoutException(self, exc_info):
    if not self.suite_config.critical and not self.suite_config.fatal_timeouts:
      return self._HandleExceptionAsWarning(exc_info)

    return super(HWTestStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    if self._CheckAborted():
      cros_build_lib.PrintBuildbotStepText('aborted')
      cros_build_lib.Warning(CQ_HWTEST_WAS_ABORTED)
      return

    build = '/'.join([self._bot_id, self.version])
    if self._run.options.remote_trybot and self._run.options.hwtest:
      debug = self._run.options.debug_forced
    else:
      debug = self._run.options.debug

    lab_status.CheckLabStatus(self._current_board)
    with timeout_util.Timeout(
        self.suite_config.timeout + constants.HWTEST_TIMEOUT_EXTENSION):
      commands.RunHWTestSuite(build,
                              self.suite_config.suite,
                              self._current_board,
                              self.suite_config.pool,
                              self.suite_config.num,
                              self.suite_config.file_bugs,
                              self.wait_for_results,
                              self.suite_config.priority,
                              self.suite_config.timeout_mins,
                              debug)


class AUTestStage(HWTestStage):
  """Stage for au hw test suites that requires special pre-processing."""

  def PerformStage(self):
    """Wait for payloads to be staged and uploads its au control files."""
    with osutils.TempDir() as tempdir:
      tarball = commands.BuildAUTestTarball(
          self._build_root, self._current_board, tempdir,
          self.version, self.upload_url)
      self.UploadArtifact(tarball)

    super(AUTestStage, self).PerformStage()


class ASyncHWTestStage(HWTestStage, generic_stages.ForgivingBuilderStage):
  """Stage that fires and forgets hw test suites to the Autotest lab."""

  def __init__(self, *args, **kwargs):
    super(ASyncHWTestStage, self).__init__(*args, **kwargs)
    self.wait_for_results = False


class QATestStage(HWTestStage, generic_stages.ForgivingBuilderStage):
  """Stage that runs qav suite in lab, blocking build but forgiving failures.
  """

  def __init__(self, *args, **kwargs):
    super(QATestStage, self).__init__(*args, **kwargs)
