# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the test stages."""

from __future__ import print_function

import collections
import math
import os

from chromite.cbuildbot import afdo
from chromite.cbuildbot import cbuildbot_run
from chromite.cbuildbot import commands
from chromite.cbuildbot import validation_pool
from chromite.cbuildbot.stages import generic_stages
from chromite.lib import cgroups
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import gs
from chromite.lib import hwtest_results
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

_GCE_TEST_ERROR_MSG = """
!!!GCETests failed!!!

Logs are uploaded in the corresponding %(gce_test_results)s. This can be found
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

  # If the unit tests take longer than 90 minutes, abort. They usually take
  # thirty minutes to run, but they can take twice as long if the machine is
  # under load (e.g. in canary groups).
  #
  # If the processes hang, parallel_emerge will print a status report after 60
  # minutes, so we picked 90 minutes because it gives us a little buffer time.
  UNIT_TEST_TIMEOUT = 90 * 60

  def PerformStage(self):
    extra_env = {}
    if self._run.config.useflags:
      extra_env['USE'] = ' '.join(self._run.config.useflags)
    r = ' Reached UnitTestStage timeout.'
    with timeout_util.Timeout(self.UNIT_TEST_TIMEOUT, reason_message=r):
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
    if not self._run.options.vmtests:
      return

    test_results_root = commands.CreateTestRoot(self._build_root)
    test_basename = constants.VM_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      for vm_test in self._run.config.vm_tests:
        test_type = vm_test.test_type
        logging.info('Running VM test %s.', test_type)
        per_test_results_dir = os.path.join(test_results_root, test_type)
        with cgroups.SimpleContainChildren('VMTest'):
          r = ' Reached VMTestStage test run timeout.'
          with timeout_util.Timeout(vm_test.timeout, reason_message=r):
            self._RunTest(test_type, per_test_results_dir)

    except Exception:
      logging.error(_VM_TEST_ERROR_MSG % dict(vm_test_results=test_basename))
      self._ArchiveVMFiles(test_results_root)
      raise
    finally:
      self._ArchiveTestResults(test_results_root, test_basename)


class GCETestStage(VMTestStage):
  """Run autotests on a GCE VM instance."""

  config_name = 'gce_tests'

  # TODO: We should revisit whether GCE tests should have their own configs.
  TEST_TIMEOUT = 60 * 60

  def _RunTest(self, test_type, test_results_dir):
    """Run a GCE test.

    Args:
      test_type: Any test in constants.VALID_GCE_TEST_TYPES
      test_results_dir: The base directory to store the results.
    """
    image_path = os.path.join(self.GetImageDirSymlink(),
                              constants.TEST_IMAGE_GCE_TAR)
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
    test_results_root = commands.CreateTestRoot(self._build_root)
    test_basename = constants.GCE_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      for gce_test in self._run.config.gce_tests:
        test_type = gce_test.test_type
        logging.info('Running GCE test %s.', test_type)
        per_test_results_dir = os.path.join(test_results_root, test_type)
        with cgroups.SimpleContainChildren('GCETest'):
          r = ' Reached GCETestStage test run timeout.'
          with timeout_util.Timeout(self.TEST_TIMEOUT, reason_message=r):
            self._RunTest(gce_test.test_type, per_test_results_dir)

    except Exception:
      logging.error(_GCE_TEST_ERROR_MSG % dict(gce_test_results=test_basename))
      raise
    finally:
      self._ArchiveTestResults(test_results_root, test_basename)


class HWTestStage(generic_stages.BoardSpecificBuilderStage,
                  generic_stages.ArchivingStageMixin):
  """Stage that runs tests in the Autotest lab."""

  option_name = 'tests'
  config_name = 'hw_tests'
  stage_name = 'HWTest'

  PERF_RESULTS_EXTENSION = 'results'

  def __init__(
      self, builder_run, board, model, suite_config, suffix=None, **kwargs):

    if suffix is None:
      suffix = ''

    if board is not model:
      suffix += ' [%s]' % (model)

    if not self.TestsEnabled(builder_run):
      suffix += ' [DISABLED]'

    suffix = self.UpdateSuffix(suite_config.suite, suffix)
    super(HWTestStage, self).__init__(builder_run, board,
                                      suffix=suffix,
                                      **kwargs)
    if not self._run.IsToTBuild():
      suite_config.SetBranchedValues()

    self.suite_config = suite_config
    self.wait_for_results = True

    self._model = model

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
        logging.info('HWTest did not run because the board was not '
                     'available in the lab yet')
        return self._HandleExceptionAsSuccess(exc_info)

    return super(HWTestStage, self)._HandleStageException(exc_info)

  def GenerateSubsysResult(self, json_dump_dict, subsystems):
    """Generate the pass/fail subsystems dict.

    Args:
      json_dump_dict: the parsed json_dump dictionary.
      subsystems: A set of subsystems that current board will test.

    Returns:
      A tuple, first element is the pass subsystem set; the second is the fail
      subsystem set
    """
    if not subsystems or not json_dump_dict:
      return None

    pass_subsystems = set()
    fail_subsystems = set()
    for test_result in json_dump_dict.get('tests', dict()).values():
      test_subsys = set([attr[10:] for attr in test_result.get('attributes')
                         if attr.startswith('subsystem:')])
      # Only track the test result of the subsystems current board tests.
      target_subsys = subsystems & test_subsys
      if test_result.get('status') == 'GOOD':
        pass_subsystems |= target_subsys
      else:
        fail_subsystems |= target_subsys
    pass_subsystems -= fail_subsystems
    return (pass_subsystems, fail_subsystems)

  def ReportHWTestResults(self, json_dump_dict, build_id, db):
    """Report HWTests results to cidb.

    Args:
      json_dump_dict: A dict containing the command json dump results.
      build_id: The build id (string) of this build.
      db: An instance of cidb.CIDBConnection.

    Returns:
      How many results are reported to CIDB.
    """
    if not json_dump_dict:
      logging.info('No json dump found, no HWTest results to report')
      return

    if not db:
      logging.info('No DB instance found, not reporting HWTest results.')
      return

    results = []
    for test_name, value in json_dump_dict.get('tests', dict()).iteritems():
      status = value.get('status')
      result = constants.HWTEST_STATUS_OTHER
      if status == 'GOOD':
        result = constants.HWTEST_STATUS_PASS
      elif status == 'FAIL':
        result = constants.HWTEST_STATUS_FAIL
      elif status == 'ABORT':
        result = constants.HWTEST_STATUS_ABORT
      else:
        logging.info('Unknown status for test %s:%s', test_name, result)

      results.append(hwtest_results.HWTestResult.FromReport(
          build_id, test_name, result))

    if results:
      logging.info('Reporting hwtest results: %s ', results)
      db.InsertHWTestResults(results)

    return len(results)

  def WaitUntilReady(self):
    """Wait until payloads and test artifacts are ready or not."""
    # Wait for UploadHWTestArtifacts to generate and upload the artifacts.
    if not self.GetParallel('test_artifacts_uploaded',
                            pretty_name='payloads and test artifacts'):
      logging.PrintBuildbotStepWarnings()
      logging.warning('missing test artifacts')
      logging.warning('Cannot run %s because UploadTestArtifacts failed. '
                      'See UploadTestArtifacts for details.' % self.stage_name)
      return False

    return True

  def TestsEnabled(self, builder_run):
    """Abstract the logic to decide if tests are enabled."""
    if (builder_run.options.remote_trybot and
        (builder_run.options.hwtest or builder_run.config.pre_cq)):
      return not builder_run.options.debug_forced
    else:
      return not builder_run.options.debug

  def PerformStage(self):
    if self.suite_config.suite == constants.HWTEST_AFDO_SUITE:
      arch = self._GetPortageEnvVar('ARCH', self._current_board)
      cpv = portage_util.BestVisible(constants.CHROME_CP,
                                     buildroot=self._build_root)
      afdo.InitGSUrls(self._current_board)
      if afdo.CheckAFDOPerfData(cpv, arch, gs.GSContext()):
        logging.info('AFDO profile already generated for arch %s '
                     'and Chrome %s. Not generating it again',
                     arch, cpv.version_no_rev.split('_')[0])
        return

    if self.suite_config.suite in [constants.HWTEST_CTS_QUAL_SUITE,
                                   constants.HWTEST_GTS_QUAL_SUITE]:
      # Increase the priority for CTS/GTS qualification suite as we want stable
      # build to have higher priority than beta build (again higher than dev).
      try:
        cros_vers = self._run.GetVersionInfo().VersionString().split('.')
        if not isinstance(self.suite_config.priority, (int, long)):
          # Convert CTS/GTS priority to corresponding integer value.
          self.suite_config.priority = constants.HWTEST_PRIORITIES_MAP[
              self.suite_config.priority]
        # We add 1/10 of the branch version to the priority. This results in a
        # modest priority bump the older the branch is. Typically beta priority
        # would be dev + [1..4] and stable priority dev + [5..9].
        self.suite_config.priority += int(math.ceil(float(cros_vers[1]) / 10.0))
      except cbuildbot_run.VersionNotSetError:
        logging.debug('Could not obtain version info. %s will use initial '
                      'priority value: %s', self.suite_config.suite,
                      self.suite_config.priority)

    build = '/'.join([self._bot_id, self.version])

    # Get the subsystems set for the board to test
    per_board_dict = self._run.attrs.metadata.GetDict()['board-metadata']
    current_board_dict = per_board_dict.get(self._current_board)
    if current_board_dict:
      subsystems = set(current_board_dict.get('subsystems_to_test', []))
      # 'subsystem:all' indicates to skip the subsystem logic
      if 'all' in subsystems:
        subsystems = None
    else:
      subsystems = None

    skip_duts_check = False
    if config_lib.IsCanaryType(self._run.config.build_type):
      skip_duts_check = True

    build_id, db = self._run.GetCIDBHandle()

    cmd_result = commands.RunHWTestSuite(
        build, self.suite_config.suite, self._model,
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
        debug=not self.TestsEnabled(self._run),
        subsystems=subsystems, skip_duts_check=skip_duts_check,
        job_keyvals=self.GetJobKeyvals())

    if config_lib.IsCQType(self._run.config.build_type):
      self.ReportHWTestResults(cmd_result.json_dump_result, build_id, db)

    subsys_tuple = self.GenerateSubsysResult(cmd_result.json_dump_result,
                                             subsystems)
    if db:
      if not subsys_tuple:
        db.InsertBuildMessage(build_id, message_type=constants.SUBSYSTEMS,
                              message_subtype=constants.SUBSYSTEM_UNUSED,
                              board=self._current_board)
      else:
        logging.info('pass_subsystems: %s, fail_subsystems: %s',
                     subsys_tuple[0], subsys_tuple[1])
        for s in subsys_tuple[0]:
          db.InsertBuildMessage(build_id, message_type=constants.SUBSYSTEMS,
                                message_subtype=constants.SUBSYSTEM_PASS,
                                message_value=str(s), board=self._current_board)
        for s in subsys_tuple[1]:
          db.InsertBuildMessage(build_id, message_type=constants.SUBSYSTEMS,
                                message_subtype=constants.SUBSYSTEM_FAIL,
                                message_value=str(s), board=self._current_board)
    if cmd_result.to_raise:
      raise cmd_result.to_raise


class AUTestStage(HWTestStage):
  """Stage for au hw test suites that requires special pre-processing."""

  stage_name = "AUTest"

  def PerformStage(self):
    """Uploads its au control files."""
    with osutils.TempDir() as tempdir:
      tarball = commands.BuildAUTestTarball(
          self._build_root, self._current_board, tempdir,
          self.version, self.upload_url)
      self.UploadArtifact(tarball)

    super(AUTestStage, self).PerformStage()


class ForgivenVMTestStage(VMTestStage, generic_stages.ForgivingBuilderStage):
  """Stage that forgives vm test failures."""

  stage_name = "ForgivenVMTest"

  def __init__(self, *args, **kwargs):
    super(ForgivenVMTestStage, self).__init__(*args, **kwargs)


class ASyncHWTestStage(HWTestStage, generic_stages.ForgivingBuilderStage):
  """Stage that fires and forgets hw test suites to the Autotest lab."""

  stage_name = "ASyncHWTest"

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
      self._UploadPerfValues(perf_values, platform_name, test_name,
                             cros_version=cros_ver,
                             chrome_version=chrome_ver)


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


class CrosSigningTestStage(generic_stages.BuilderStage):
  """Stage that runs the signer unittests.

  This requires an internal source code checkout.
  """

  def PerformStage(self):
    """Run the cros-signing unittests."""
    commands.RunCrosSigningTests(self._build_root)


class ChromiteTestStage(generic_stages.BuilderStage):
  """Stage that runs Chromite tests, including network tests."""

  def PerformStage(self):
    """Run the cros-signing unittests."""
    commands.RunChromiteTests(self._build_root, network=False)
