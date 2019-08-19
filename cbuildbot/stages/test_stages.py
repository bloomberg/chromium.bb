# -*- coding: utf-8 -*-
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
from chromite.lib import config_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import failures_lib
from chromite.lib import gs
from chromite.lib import hwtest_results
from chromite.lib import image_test_lib
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import parallel
from chromite.lib import perf_uploader
from chromite.lib import portage_util
from chromite.lib import timeout_util

PRE_CQ = validation_pool.PRE_CQ


class UnitTestStage(generic_stages.BoardSpecificBuilderStage,
                    generic_stages.ArchivingStageMixin):
  """Run unit tests."""

  option_name = 'tests'
  config_name = 'unittests'
  category = constants.PRODUCT_OS_STAGE

  # If the unit tests take longer than 90 minutes, abort. They usually take
  # thirty minutes to run, but they can take twice as long if the machine is
  # under load (e.g. in canary groups).
  #
  # If the processes hang, parallel_emerge will print a status report after 60
  # minutes, so we picked 90 minutes because it gives us a little buffer time.
  UNIT_TEST_TIMEOUT = 90 * 60

  def WaitUntilReady(self):
    """Block until UploadTestArtifacts completes.

    The attribute 'test_artifacts_uploaded' is set by UploadTestArtifacts.

    Returns:
      Boolean that authorizes running this stage.
    """
    self.board_runattrs.GetParallel('test_artifacts_uploaded',
                                    timeout=None)
    return True

  def HandleSkip(self):
    """Launch DebugSymbolsStage if UnitTestStage is skipped."""
    self.board_runattrs.SetParallel('unittest_completed', True)
    return super(UnitTestStage, self).HandleSkip()

  def _HandleStageException(self, exc_info):
    """Launch DebugSymbolStage if UnitTestStage is raising an exception."""
    self.board_runattrs.SetParallel('unittest_completed', True)
    return super(UnitTestStage, self)._HandleStageException(exc_info)

  def PerformStage(self):
    extra_env = {}
    if self._run.config.useflags:
      extra_env['USE'] = ' '.join(self._run.config.useflags)
    r = ' Reached UnitTestStage timeout.'
    with timeout_util.Timeout(self.UNIT_TEST_TIMEOUT, reason_message=r):
      commands.RunUnitTests(
          self._build_root,
          self._current_board,
          blacklist=self._run.config.unittest_blacklist,
          extra_env=extra_env,
          build_stage=self._run.config.build_packages)
    # The attribute 'unittest_completed' is used in DebugSymbolsStage.
    self.board_runattrs.SetParallel('unittest_completed', True)
    # Package UnitTest binaries.
    tarball = commands.BuildUnitTestTarball(
        self._build_root, self._current_board, self.archive_path)
    self.UploadArtifact(tarball, archive=False)


class HWTestStage(generic_stages.BoardSpecificBuilderStage,
                  generic_stages.ArchivingStageMixin):
  """Stage that runs tests in the Autotest lab."""

  option_name = 'tests'
  config_name = 'hw_tests'
  stage_name = 'HWTest'
  category = constants.TEST_INFRA_STAGE

  PERF_RESULTS_EXTENSION = 'results'

  def __init__(self,
               builder_run,
               buildstore,
               board,
               model,
               suite_config,
               suffix=None,
               lab_board_name=None,
               **kwargs):

    if suffix is None:
      suffix = ''

    if model:
      suffix += ' [%s]' % (model)

    if not self.TestsEnabled(builder_run):
      suffix += ' [DISABLED]'

    suffix = self.UpdateSuffix(suite_config.suite, suffix)
    super(HWTestStage, self).__init__(
        builder_run, buildstore, board, suffix=suffix, **kwargs)
    if not self._run.IsToTBuild():
      self._SetBranchedSuiteConfig(suite_config)

    self.suite_config = suite_config
    self.wait_for_results = True

    self._model = model
    self._board_name = lab_board_name or board

  def _SetBranchedSuiteConfig(self, suite_config):
    suite_config.SetBranchedValues()

  # Disable complaint about calling _HandleStageException.
  # pylint: disable=protected-access
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
    for test_name, value in json_dump_dict.get('tests', dict()).items():
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

      results.append(
          hwtest_results.HWTestResult.FromReport(build_id, test_name, result))

    if results:
      logging.info('Reporting hwtest results: %s ', results)
      db.InsertHWTestResults(results)

    return len(results)

  def WaitUntilReady(self):
    """Wait until payloads and test artifacts are ready or not."""
    # Wait for UploadHWTestArtifacts to generate and upload the artifacts.
    if not self.GetParallel(
        'test_artifacts_uploaded', pretty_name='payloads and test artifacts'):
      logging.PrintBuildbotStepWarnings()
      logging.warning('missing test artifacts')
      logging.warning(
          'Cannot run %s because UploadTestArtifacts failed. '
          'See UploadTestArtifacts for details.', self.stage_name)
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
      cpv = portage_util.PortageqBestVisible(
          constants.CHROME_CP, cwd=self._build_root)
      if afdo.CheckAFDOPerfData(cpv, arch, gs.GSContext()):
        logging.info(
            'AFDO profile already generated for arch %s '
            'and Chrome %s. Not generating it again', arch,
            cpv.version_no_rev.split('_')[0])
        return

    if self.suite_config.suite in [
        constants.HWTEST_CTS_QUAL_SUITE, constants.HWTEST_GTS_QUAL_SUITE
    ]:
      # Increase the priority for CTS/GTS qualification suite as we want stable
      # build to have higher priority than beta build (again higher than dev).
      try:
        cros_vers = self._run.GetVersionInfo().VersionString().split('.')
        # Convert priority to corresponding integer value.
        self.suite_config.priority = constants.HWTEST_PRIORITIES_MAP[
            self.suite_config.priority]
        # We add 1/10 of the branch version to the priority. This results in a
        # modest priority bump the older the branch is. Typically beta priority
        # would be dev + [1..4] and stable priority dev + [5..9].
        self.suite_config.priority += int(math.ceil(float(cros_vers[1]) / 10.0))
      except cbuildbot_run.VersionNotSetError:
        logging.debug(
            'Could not obtain version info. %s will use initial '
            'priority value: %s', self.suite_config.suite,
            self.suite_config.priority)

    build = '/'.join([self._bot_id, self.version])

    skip_duts_check = False
    if config_lib.IsCanaryType(self._run.config.build_type):
      skip_duts_check = True

    build_identifier, db = self._run.GetCIDBHandle()
    build_id = build_identifier.cidb_id

    test_args = None
    if config_lib.IsCQType(self._run.config.build_type):
      test_args = {'fast': 'True'}

    cmd_result = commands.RunHWTestSuite(
        build,
        self.suite_config.suite,
        self._board_name,
        model=self._model,
        pool=self.suite_config.pool,
        file_bugs=self.suite_config.file_bugs,
        wait_for_results=self.wait_for_results,
        priority=self.suite_config.priority,
        timeout_mins=self.suite_config.timeout_mins,
        retry=self.suite_config.retry,
        max_retries=self.suite_config.max_retries,
        minimum_duts=self.suite_config.minimum_duts,
        suite_min_duts=self.suite_config.suite_min_duts,
        suite_args=self.suite_config.suite_args,
        offload_failures_only=self.suite_config.offload_failures_only,
        debug=not self.TestsEnabled(self._run),
        skip_duts_check=skip_duts_check,
        job_keyvals=self.GetJobKeyvals(),
        test_args=test_args)

    if config_lib.IsCQType(self._run.config.build_type):
      self.ReportHWTestResults(cmd_result.json_dump_result, build_id, db)

    if cmd_result.to_raise:
      raise cmd_result.to_raise


class SkylabHWTestStage(HWTestStage):
  """Stage that runs tests in the Autotest lab with Skylab."""

  stage_name = 'SkylabHWTest'
  category = constants.TEST_INFRA_STAGE

  def _SetBranchedSuiteConfig(self, suite_config):
    suite_config.SetBranchedValuesForSkylab()

  # Temporary hack during skylab and quotascheduler migration, to override
  # certain builders' pool config values.
  def _PoolHack(self, pool):
    # Override the pool for all pfqs (except those also running release in
    # Skylab) to use the quotascheduler pool. See crbug.com/950017
    blacklist = ('asuka', 'coral', 'nyan_blaze', 'reef')
    build_type = self._run.config.build_type
    if config_lib.IsPFQType(build_type) and self._board_name not in blacklist:
      pool = constants.HWTEST_QUOTA_POOL

    # Retain all moblab boards on cq pool, until it is able
    # to migrate to Skylab. See crbug.com/949217
    if ('moblab' in self._board_name and
        config_lib.IsCQType(self._run.config.build_type)):
      pool = constants.HWTEST_PALADIN_POOL

    return pool

  # Temporary measure during skylab and quotascheduler migration, because
  # quota_account is not yet correctly specified in most HWTestConfig entries.
  def _InferQuotaAccount(self):
    """Attempt to infer quota account to use for this test, if applicable."""
    build_type = self._run.config.build_type
    # The order of checks matters here, because CQ builds are considered to be
    # PFQ type as well.
    if config_lib.IsCQType(build_type):
      return 'cq'
    if config_lib.IsPFQType(build_type):
      return 'pfq'
    return None

  def PerformStage(self):
    build = '/'.join([self._bot_id, self.version])

    pool = self._PoolHack(self.suite_config.pool)
    quota_account = self.suite_config.quota_account or self._InferQuotaAccount()

    cmd_result = commands.RunSkylabHWTestSuite(
        build,
        self.suite_config.suite,
        self._board_name,
        model=self._model,
        pool=pool,
        wait_for_results=self.wait_for_results,
        priority=self.suite_config.priority,
        timeout_mins=self.suite_config.timeout_mins,
        retry=self.suite_config.retry,
        max_retries=self.suite_config.max_retries,
        suite_args=self.suite_config.suite_args,
        job_keyvals=self.GetJobKeyvals(),
        quota_account=quota_account)

    if cmd_result.to_raise:
      raise cmd_result.to_raise


class ASyncHWTestStage(HWTestStage, generic_stages.ForgivingBuilderStage):
  """Stage that fires and forgets hw test suites to the Autotest lab."""

  stage_name = 'ASyncHWTest'
  category = constants.TEST_INFRA_STAGE

  def __init__(self, *args, **kwargs):
    super(ASyncHWTestStage, self).__init__(*args, **kwargs)
    self.wait_for_results = False


class ASyncSkylabHWTestStage(SkylabHWTestStage,
                             generic_stages.ForgivingBuilderStage):
  """Stage that fires and forgets skylab hw test suites to the Autotest lab."""

  stage_name = 'ASyncSkylabHWTest'
  category = constants.TEST_INFRA_STAGE

  def __init__(self, *args, **kwargs):
    super(ASyncSkylabHWTestStage, self).__init__(*args, **kwargs)
    self.wait_for_results = False


class ImageTestStage(generic_stages.BoardSpecificBuilderStage,
                     generic_stages.ArchivingStageMixin):
  """Stage that launches tests on the produced disk image."""

  option_name = 'image_test'
  config_name = 'image_test'
  category = constants.CI_INFRA_STAGE

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
    for test_name, perf_values in perf_entries.items():
      self._UploadPerfValues(
          perf_values,
          platform_name,
          test_name,
          cros_version=cros_ver,
          chrome_version=chrome_ver)


class BinhostTestStage(generic_stages.BuilderStage):
  """Stage that verifies Chrome prebuilts."""

  config_name = 'binhost_test'
  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    # Verify our binhosts.
    # Don't check for incremental compatibility when we uprev chrome.
    incremental = not (self._run.config.chrome_rev or
                       self._run.options.chrome_rev)
    commands.RunBinhostTest(self._build_root, incremental=incremental)


class CrosSigningTestStage(generic_stages.BuilderStage):
  """Stage that runs the signer unittests.

  This requires an internal source code checkout.
  """

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    """Run the cros-signing unittests."""
    commands.RunCrosSigningTests(self._build_root)


class UnexpectedTryjobResult(Exception):
  """Thrown if a nested tryjob passes or fails unexpectedly."""


class CbuildbotLaunchTestBuildStage(generic_stages.BuilderStage):
  """Perform a single build with cbuildbot_launch."""
  category = constants.CI_INFRA_STAGE

  def __init__(self,
               builder_run,
               buildstore,
               tryjob_buildroot,
               branch,
               build_config,
               expect_success=True,
               **kwargs):
    """Init.

    Args:
      builder_run: See builder_run on ArchiveStage
      buildstore: BuildStore instance to make DB calls with.
      tryjob_buildroot: buildroot to use for test build, NOT current build.
      branch: Branch to build. None means 'current' branch.
      build_config: Name of build config to build.
      expect_success: Is the test build expected to pass?
    """
    super(CbuildbotLaunchTestBuildStage, self).__init__(builder_run, buildstore,
                                                        **kwargs)

    self.build_config = build_config
    self.tryjob_buildroot = tryjob_buildroot
    self.branch = branch
    self.expect_success = expect_success

  def PerformStage(self):
    args = ['--branch', self.branch]
    if self._run.options.git_cache_dir:
      args.extend(['--git-cache-dir', self._run.options.git_cache_dir])

    try:
      commands.RunLocalTryjob(self._build_root, self.build_config, args,
                              self.tryjob_buildroot)
      if not self.expect_success:
        raise UnexpectedTryjobResult('Build passed unexpectedly.')
    except failures_lib.BuildScriptFailure:
      if self.expect_success:
        raise UnexpectedTryjobResult('Build failed unexpectedly.')


class CbuildbotLaunchTestStage(generic_stages.BuilderStage):
  """Stage that runs Chromite tests, including network tests."""
  category = constants.CI_INFRA_STAGE

  def __init__(self, builder_run, buildstore, **kwargs):
    """Init.

    Args:
      builder_run: See builder_run on ArchiveStage
      buildstore: BuildStore instance to make DB calls with.
    """
    super(CbuildbotLaunchTestStage, self).__init__(builder_run, buildstore,
                                                   **kwargs)
    self.tryjob_buildroot = None

  def RunCbuildbotLauncher(self, suffix, branch, build_config, expect_success):
    """Run a new stage to test a cbuildbot_launch subbuild."""
    substage = CbuildbotLaunchTestBuildStage(
        self._run,
        self.buildstore,
        tryjob_buildroot=self.tryjob_buildroot,
        branch=branch,
        build_config=build_config,
        expect_success=expect_success,
        suffix=suffix)
    substage.Run()

  def PerformStage(self):
    """Run sample cbuildbot_launch tests."""
    # TODO: Move this tempdir, it's a problem.
    with osutils.TempDir() as tryjob_buildroot:
      self.tryjob_buildroot = tryjob_buildroot

      # Iniitial build fails.
      self.RunCbuildbotLauncher(
          'Initial Build (fail)',
          self._run.options.branch,
          'fail-build',
          expect_success=False)

      # Test cleanup after a fail.
      self.RunCbuildbotLauncher(
          'Second Build (pass)',
          self._run.options.branch,
          'success-build',
          expect_success=True)

      # Test reduced cleanup after a pass.
      self.RunCbuildbotLauncher(
          'Third Build (pass)',
          self._run.options.branch,
          'success-build',
          expect_success=True)

      # Test branch transition.
      self.RunCbuildbotLauncher(
          'Branch Build (pass)',
          'release-R68-10718.B',
          'success-build',
          expect_success=True)


class ChromiteTestStage(generic_stages.BuilderStage):
  """Stage that runs Chromite tests, excluding network tests."""

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    """Run the chromite unittests."""
    buildroot_chromite = path_util.ToChrootPath(
        os.path.join(self._build_root, 'chromite'))

    cmd = [
        os.path.join(buildroot_chromite, 'run_tests'),
        # TODO(crbug.com/682381): When tests can pass, add '--network',
    ]
    # TODO: Remove enter_chroot=True when we have virtualenv support.
    # Until then, we skip all chromite tests outside the chroot.
    cros_build_lib.RunCommand(cmd, enter_chroot=True)


class CidbIntegrationTestStage(generic_stages.BuilderStage):
  """Stage that runs the CIDB integration tests."""

  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    """Run the CIDB integration tests."""
    buildroot_chromite = path_util.ToChrootPath(
        os.path.join(self._build_root, 'chromite'))

    cmd = [
        os.path.join(buildroot_chromite, 'lib', 'cidb_integration_test'),
        '-v',
        # '--network'  Doesn't work in a build, yet.
    ]
    cros_build_lib.RunCommand(cmd, enter_chroot=True)


class DebugInfoTestStage(generic_stages.BoardSpecificBuilderStage,
                         generic_stages.ForgivingBuilderStage):
  """Perform tests that are based on debug info

  Tests may include, for example,
    * whether dwarf info exists
    * whether clang is used
    * whether FORTIFY is enabled, etc.
  """

  option_name = 'tests'
  category = constants.CI_INFRA_STAGE

  def PerformStage(self):
    cmd = [
        'debug_info_test',
        os.path.join(
            cros_build_lib.GetSysroot(board=self._current_board),
            'usr/lib/debug')
    ]
    cros_build_lib.RunCommand(cmd, enter_chroot=True)


class TestPlanStage(generic_stages.BoardSpecificBuilderStage):
  """Stage that constructs test plans."""

  def __init__(self, builder_run, buildstore, board, **kwargs):

    super(TestPlanStage, self).__init__(builder_run, buildstore, board,
                                        **kwargs)

  def WaitUntilReady(self):
    config = self._run.config
    return bool('hw_tests' in config and config.hw_tests)

  def PerformStage(self):
    builder_run = self._run
    board = self._current_board

    if not builder_run.options.archive:
      logging.warning('HWTests were requested but could not be run because '
                      "artifacts weren't uploaded. Please ensure the archive "
                      'option in the builder config is set to True.')
      return


    models = [config_lib.ModelTestConfig(
        None, config_lib.GetNonUniBuildLabBoardName(board))]
    if builder_run.config.models:
      models = builder_run.config.models

    parallel_stages = []
    for suite_config in builder_run.config.hw_tests:
      # Even for blocking stages, all models can still be run in parallel since
      # it will still block the next stage from executing.
      for model in models:
        new_stage = self._GetHWTestStage(builder_run, self.buildstore, board,
                                         model, suite_config)
        if new_stage:
          parallel_stages.append(new_stage)

      # Please see docstring for blocking in the HWTestConfig for more
      # information on this behavior.
      # Expected behavior:
      #     1) Blocking suites are kicked off first, e.g. provision suite.
      #     2) If it's unibuild, the blocking suites of all models are kicked
      #        off in parallel first.
      if suite_config.blocking:
        steps = [stage.Run for stage in parallel_stages]
        parallel.RunParallelSteps(steps)
        parallel_stages = []

    if parallel_stages:
      steps = [stage.Run for stage in parallel_stages]
      parallel.RunParallelSteps(steps)

  def _GetHWTestStage(self, builder_run, buildstore, board, model,
                      suite_config):
    """Gets the correct hw test stage for a given test suite and model.

    Args:
      builder_run: BuilderRun object for these background stages.
      buildstore: BuildStore instance to make DB calls with.
      board: board overlay name
      model: ModelTestConfig object to test against.
      suite_config: HWTestConfig object that defines the test suite.

    Returns:
      The test stage or None if the test suite was filtered for the model.
    """
    result = None

    # If test_suites doesn't exist, then there is no filter.
    # Whereas, an empty array will act as a comprehensive filter.
    if model.test_suites is None or suite_config.suite in model.test_suites:
      stage_class = None
      if suite_config.async:
        stage_class = ASyncHWTestStage
      else:
        stage_class = HWTestStage

      hwtest_env = config_lib.GetHWTestEnv(builder_run.config,
                                           model_config=model,
                                           suite_config=suite_config)
      if hwtest_env == constants.ENV_SKYLAB:
        if suite_config.async:
          stage_class = ASyncSkylabHWTestStage
        else:
          stage_class = SkylabHWTestStage

      result = stage_class(
          builder_run,
          buildstore,
          board,
          model.name,
          suite_config,
          lab_board_name=model.lab_board_name)
    return result
