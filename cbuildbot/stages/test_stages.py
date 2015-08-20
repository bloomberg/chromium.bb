# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the test stages."""

from __future__ import print_function

import collections
import os

from chromite.cbuildbot import afdo
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot import config_lib
from chromite.cbuildbot import constants
from chromite.cbuildbot import failures_lib
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cgroups
from chromite.lib import cros_logging as logging
from chromite.lib import gs
from chromite.lib import image_test_lib
from chromite.lib import osutils
from chromite.lib import perf_uploader
from chromite.lib import portage_util
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
  # Check if the GCS target is available every 15 seconds.
  CHECK_GCS_PERIOD = 15
  CHECK_GCS_TIMEOUT = VM_TEST_TIMEOUT

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

  def _NoTestResults(self, path):
    """Returns True if |path| is not a directory or is an empty directory."""
    return not os.path.isdir(path) or not os.listdir(path)

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def _ArchiveTestResults(self, test_results_dir, test_basename):
    """Archives test results to Google Storage.

    Args:
      test_results_dir: Name of the directory containing the test results.
      test_basename: The basename to archive the tests.
    """
    results_path = commands.GetTestResultsDir(
        self._build_root, test_results_dir)

    # Skip archiving if results_path does not exist or is an empty directory.
    if self._NoTestResults(results_path):
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

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def _ArchiveVMFiles(self, test_results_dir):
    vm_files = commands.ArchiveVMFiles(
        self._build_root, os.path.join(test_results_dir, 'test_harness'),
        self.archive_path)
    # We use paths relative to |self.archive_path|, for prettier
    # formatting on the web page.
    self._Upload([os.path.basename(image) for image in vm_files])

  def _Upload(self, filenames):
    logging.info('Uploading artifacts to Google Storage...')
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
      if test_type == constants.GCE_VM_TEST_TYPE:
        image_path = os.path.join(self.GetImageDirSymlink(),
                                  constants.TEST_IMAGE_GCE_TAR)
      else:
        image_path = os.path.join(self.GetImageDirSymlink(),
                                  constants.TEST_IMAGE_BIN)
      ssh_private_key = os.path.join(self.GetImageDirSymlink(),
                                     constants.TEST_KEY_PRIVATE)
      if not os.path.exists(ssh_private_key):
        # TODO: Disallow usage of default test key completely.
        logging.warning('Test key was not found in the image directory. '
                        'Default key will be used.')
        ssh_private_key = None

      commands.RunTestSuite(self._build_root,
                            self._current_board,
                            image_path,
                            os.path.join(test_results_dir, 'test_harness'),
                            test_type=test_type,
                            whitelist_chrome_crashes=self._chrome_rev is None,
                            archive_dir=self.bot_archive_root,
                            ssh_private_key=ssh_private_key)

  def PerformStage(self):
    # These directories are used later to archive test artifacts.
    test_results_dir = commands.CreateTestRoot(self._build_root)
    test_basename = constants.VM_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      for test_type in self._run.config.vm_tests:
        logging.info('Running VM test %s.', test_type)
        with cgroups.SimpleContainChildren('VMTest'):
          with timeout_util.Timeout(self.VM_TEST_TIMEOUT):
            self._RunTest(test_type, test_results_dir)

    except Exception:
      logging.error(_VM_TEST_ERROR_MSG % dict(vm_test_results=test_basename))
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

  def __init__(self, builder_run, board, suite_config, suffix=None, **kwargs):
    suffix = self.UpdateSuffix(suite_config.suite, suffix)
    super(HWTestStage, self).__init__(builder_run, board,
                                      suffix=suffix,
                                      **kwargs)
    if not self._run.IsToTBuild():
      suite_config.SetBranchedValues()

    self.suite_config = suite_config
    self.wait_for_results = True

  # Disable complaint about calling _HandleStageException.
  # pylint: disable=W0212
  def _HandleStageException(self, exc_info):
    """Override and don't set status to FAIL but FORGIVEN instead."""
    exc_type = exc_info[0]

    # If the suite config says HW Tests can only warn, only warn.
    if self.suite_config.warn_only:
      return self._HandleExceptionAsWarning(exc_info)

    if self.suite_config.critical:
      return super(HWTestStage, self)._HandleStageException(exc_info)

    if issubclass(exc_type, failures_lib.TestWarning):
      # HWTest passed with warning. All builders should pass.
      logging.warning('HWTest passed with warning code.')
      return self._HandleExceptionAsWarning(exc_info)
    elif issubclass(exc_type, failures_lib.BoardNotAvailable):
      # Some boards may not have been setup in the lab yet for
      # non-code-checkin configs.
      if not config_lib.IsPFQType(self._run.config.build_type):
        logging.warning('HWTest did not run because the board was not '
                        'available in the lab yet')
        return self._HandleExceptionAsWarning(exc_info)

    return super(HWTestStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    # Wait for UploadHWTestArtifacts to generate the payloads.
    if not self.GetParallel('payloads_generated', pretty_name='payloads'):
      logging.PrintBuildbotStepWarnings('missing payloads')
      logging.warning('Cannot run HWTest because UploadTestArtifacts failed. '
                      'See UploadTestArtifacts for details.')
      return

    if self.suite_config.suite == constants.HWTEST_AFDO_SUITE:
      arch = self._GetPortageEnvVar('ARCH', self._current_board)
      cpv = portage_util.BestVisible(constants.CHROME_CP,
                                     buildroot=self._build_root)
      if afdo.CheckAFDOPerfData(cpv, arch, gs.GSContext()):
        logging.info('AFDO profile already generated for arch %s '
                     'and Chrome %s. Not generating it again',
                     arch, cpv.version_no_rev.split('_')[0])
        return

    build = '/'.join([self._bot_id, self.version])
    if (self._run.options.remote_trybot and (self._run.options.hwtest or
                                             self._run.config.pre_cq)):
      debug = self._run.options.debug_forced
    else:
      debug = self._run.options.debug

    # Get the subsystems set for the board to test
    per_board_dict = self._run.attrs.metadata.GetDict()['board-metadata']
    current_board_dict = per_board_dict.get(self._current_board)
    if current_board_dict:
      subsystems = set(current_board_dict.get('subsystems_to_test', []))
    else:
      subsystems = None

    commands.RunHWTestSuite(
        build, self.suite_config.suite, self._current_board,
        pool=self.suite_config.pool, num=self.suite_config.num,
        file_bugs=self.suite_config.file_bugs,
        wait_for_results=self.wait_for_results,
        priority=self.suite_config.priority,
        timeout_mins=self.suite_config.timeout_mins,
        retry=self.suite_config.retry,
        max_retries=self.suite_config.max_retries,
        minimum_duts=self.suite_config.minimum_duts,
        suite_min_duts=self.suite_config.suite_min_duts,
        offload_failures_only=self.suite_config.offload_failures_only,
        debug=debug, subsystems=subsystems)


class AUTestStage(HWTestStage):
  """Stage for au hw test suites that requires special pre-processing."""

  def PerformStage(self):
    """Wait for payloads to be staged and uploads its au control files."""
    # Wait for UploadHWTestArtifacts to generate the payloads.
    if not self.GetParallel('delta_payloads_generated',
                            pretty_name='delta payloads'):
      logging.PrintBuildbotStepWarnings('missing delta payloads')
      logging.warning('Cannot run HWTest because UploadTestArtifacts failed. '
                      'See UploadTestArtifacts for details.')
      return

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


class ImageTestStage(generic_stages.BoardSpecificBuilderStage,
                     generic_stages.ArchivingStageMixin):
  """Stage that launches tests on the produced disk image."""

  option_name = 'image_test'
  config_name = 'image_test'

  # Give the tests 60 minutes to run. Image tests should be really quick but
  # the umount/rmdir bug (see osutils.UmountDir) may take a long time.
  IMAGE_TEST_TIMEOUT = 60 * 60

  def __init__(self, *args, **kwargs):
    super(ImageTestStage, self).__init__(*args, **kwargs)

  def PerformStage(self):
    test_results_dir = commands.CreateTestRoot(self._build_root)
    # CreateTestRoot returns a temp directory inside chroot.
    # We bring that back out to the build root.
    test_results_dir = os.path.join(self._build_root, test_results_dir[1:])
    test_results_dir = os.path.join(test_results_dir, 'image_test_results')
    osutils.SafeMakedirs(test_results_dir)
    try:
      with timeout_util.Timeout(self.IMAGE_TEST_TIMEOUT):
        commands.RunTestImage(
            self._build_root,
            self._current_board,
            self.GetImageDirSymlink(),
            test_results_dir,
        )
    finally:
      self.SendPerfValues(test_results_dir)

  def SendPerfValues(self, test_results_dir):
    """Gather all perf values in |test_results_dir| and send them to chromeperf.

    The uploading will be retried 3 times for each file.

    Args:
      test_results_dir: A path to the directory with perf files.
    """
    # A dict of list of perf values, keyed by test name.
    perf_entries = collections.defaultdict(list)
    for root, _, filenames in os.walk(test_results_dir):
      for relative_name in filenames:
        if not image_test_lib.IsPerfFile(relative_name):
          continue
        full_name = os.path.join(root, relative_name)
        entries = perf_uploader.LoadPerfValues(full_name)
        test_name = image_test_lib.ImageTestCase.GetTestName(relative_name)
        perf_entries[test_name].extend(entries)

    platform_name = self._run.bot_id
    try:
      cros_ver = self._run.GetVersionInfo().VersionString()
    except cbuildbot_run.VersionNotSetError:
      logging.error('Could not obtain version info. '
                    'Failed to upload perf results.')
      return

    chrome_ver = self._run.DetermineChromeVersion()
    for test_name, perf_values in perf_entries.iteritems():
      try:
        perf_uploader.UploadPerfValues(perf_values, platform_name, test_name,
                                       cros_version=cros_ver,
                                       chrome_version=chrome_ver)
      except Exception:
        logging.exception('Failed to upload perf result for test %s.',
                          test_name)


class BinhostTestStage(generic_stages.BuilderStage):
  """Stage that verifies Chrome prebuilts."""

  config_name = 'binhost_test'

  def PerformStage(self):
    # Verify our binhosts.
    # Don't check for incremental compatibility when we uprev chrome.
    incremental = not (self._run.config.chrome_rev or
                       self._run.options.chrome_rev)
    commands.RunBinhostTest(self._build_root, incremental=incremental)


class BranchUtilTestStage(generic_stages.BuilderStage):
  """Stage that verifies branching works on the latest manifest version."""

  config_name = 'branch_util_test'

  def PerformStage(self):
    assert (hasattr(self._run.attrs, 'manifest_manager') and
            self._run.attrs.manifest_manager is not None), \
        'Must run ManifestVersionedSyncStage before this stage.'
    manifest_manager = self._run.attrs.manifest_manager
    commands.RunBranchUtilTest(
        self._build_root,
        manifest_manager.GetCurrentVersionInfo().VersionString())
