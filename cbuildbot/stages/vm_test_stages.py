# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing various flavours of VM test stages.

These stages all test the ChromeOS image by running it in some sort of VM.
The tests themselves may vary, as may the harness used to manage the VM.
But they all share some common operations around creating the VM image,
archiving results and VM images in case of failure.
"""

from __future__ import print_function

import fnmatch
import os
import re
import shutil

from chromite.cbuildbot import commands
from chromite.cbuildbot.stages import generic_stages
from chromite.cli.cros.tests  import cros_vm_test
from chromite.lib import cgroups
from chromite.lib import constants
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import cts_helper
from chromite.lib import failures_lib
from chromite.lib import moblab_vm
from chromite.lib import osutils
from chromite.lib import path_util
from chromite.lib import timeout_util


_GCE_TEST_RESULTS = 'gce_test_results_%(attempt)s'
_VM_TEST_RESULTS = 'vm_test_results_%(attempt)s'

_ERROR_MSG = """
!!!%(test_name)s failed!!!

Logs are uploaded in the corresponding %(test_results)s. This can be found
by clicking on the artifacts link in the "Report" Stage. Specifically look
for the test_harness/failed for the failing tests. For more
particulars, please refer to which test failed i.e. above see the
individual test that failed -- or if an update failed, check the
corresponding update directory.
"""

# For sorting through VM test results.
_TEST_REPORT_FILENAME = 'test_report.log'
_TEST_PASSED = 'PASSED'
_TEST_FAILED = 'FAILED'


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
    test_list = ListTests(results_path, show_passed=False)
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
    results_path = GetTestResultsDir(
        self._build_root, test_results_dir)

    # Skip archiving if results_path does not exist or is an empty directory.
    if self._NoTestResults(results_path):
      return

    archived_results_dir = os.path.join(self.archive_path, test_basename)
    # Copy relevant files to archvied_results_dir.
    ArchiveTestResults(results_path, archived_results_dir)
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
  def _ReportResultsToDashboards(self, test_results_dir):
    """Report VMTests results to chromeperf and CTS dashboard.

    Args:
      test_results_dir: Name of the directory containing the test results.
    """
    # TODO(pwang): also upload to sponge and afe/tko so results show up
    # consistently on all dashboards like wmatrix and goldeneye.
    results_path = GetTestResultsDir(
        self._build_root, test_results_dir)

    # Skip reporting if results_path does not exist or is an empty directory.
    if self._NoTestResults(results_path):
      logging.info('Found no test results. Skipping upload to dashboards.')
      return

    for test_name, test_dir in ListTests(results_path):
      if cts_helper.isCtsTest(test_name):
        self._ReportCtsResults(test_name, os.path.join(results_path, test_dir))

  def _ReportCtsResults(self, test_name, test_dir):
    """Report CTS/GTS result to their dashboards.

    Args:
      test_name: name of the test.
      test_dir: path to the test directory.
    """
    logging.info('Reporting cts test: %s in %s', test_name, test_dir)
    builder = self._run.GetBuilderName()
    buildbucket_id = self._run.options.buildbucket_id
    buildbucket_id = str(buildbucket_id)
    def _uploader(gs_url, file_path, *args, **kwargs):
      directory, filename = os.path.split(file_path)
      logging.info('Uploading %s to %s', file_path, gs_url)
      commands.UploadArchivedFile(
          directory, [gs_url], filename, *args, **kwargs)

    cts_helper.uploadFiles(test_dir, builder, buildbucket_id, buildbucket_id,
                           test_name, _uploader, self._run.debug,
                           update_list=False, acl=self.acl)

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def _ArchiveVMFiles(self, test_results_dir):
    vm_files = ArchiveVMFiles(
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

  def _RunTest(self, test_config, test_results_dir):
    """Run a VM test.

    Args:
      test_config: Any config_lib.VMTestConfig with test_type in
                   constants.VALID_VM_TEST_TYPES.
      test_results_dir: The base directory to store the results.
    """
    test_type = test_config.test_type
    if test_type == constants.CROS_VM_TEST_TYPE:
      RunCrosVMTest(self._current_board, self.GetImageDirSymlink())
    elif test_type == constants.DEV_MODE_TEST_TYPE:
      RunDevModeTest(
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

      RunTestSuite(
          self._build_root,
          self._current_board,
          image_path,
          os.path.join(test_results_dir, 'test_harness'),
          test_config=test_config,
          whitelist_chrome_crashes=self._chrome_rev is None,
          archive_dir=self.bot_archive_root,
          ssh_private_key=ssh_private_key,
      )

  def PerformStage(self):
    # These directories are used later to archive test artifacts.
    if not self._run.options.vmtests:
      return

    test_results_root = commands.CreateTestRoot(self._build_root)
    test_basename = _VM_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      for vm_test in self._run.config.vm_tests:
        logging.info('Running VM test %s.', vm_test.test_type)
        if vm_test.test_type == constants.VM_SUITE_TEST_TYPE:
          per_test_results_dir = os.path.join(test_results_root,
                                              vm_test.test_suite)
        else:
          per_test_results_dir = os.path.join(test_results_root,
                                              vm_test.test_type)
        with cgroups.SimpleContainChildren('VMTest'):
          r = ' Reached VMTestStage test run timeout.'
          with timeout_util.Timeout(vm_test.timeout, reason_message=r):
            self._RunTest(vm_test, per_test_results_dir)

    except Exception:
      logging.error(_ERROR_MSG % dict(test_name='VMTests',
                                      test_results=test_basename))
      self._ArchiveVMFiles(test_results_root)
      raise
    finally:
      if self._run.config.vm_test_report_to_dashboards:
        self._ReportResultsToDashboards(test_results_root)
      self._ArchiveTestResults(test_results_root, test_basename)


class ForgivenVMTestStage(VMTestStage, generic_stages.ForgivingBuilderStage):
  """Stage that forgives vm test failures."""

  stage_name = "ForgivenVMTest"

  def __init__(self, *args, **kwargs):
    super(ForgivenVMTestStage, self).__init__(*args, **kwargs)


class GCETestStage(VMTestStage):
  """Run autotests on a GCE VM instance."""

  config_name = 'gce_tests'

  # TODO: We should revisit whether GCE tests should have their own configs.
  TEST_TIMEOUT = 60 * 60

  def _RunTest(self, test_config, test_results_dir):
    """Run a GCE test.

    Args:
      test_config: Any config_lib.GCETestConfig with valid test_suite in
                   constants.VALID_GCE_TEST_SUITES.
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

    RunTestSuite(
        self._build_root,
        self._current_board,
        image_path,
        os.path.join(test_results_dir, 'test_harness'),
        test_config=test_config,
        whitelist_chrome_crashes=self._chrome_rev is None,
        archive_dir=self.bot_archive_root,
        ssh_private_key=ssh_private_key,
    )

  def PerformStage(self):
    # These directories are used later to archive test artifacts.
    test_results_root = commands.CreateTestRoot(self._build_root)
    test_basename = _GCE_TEST_RESULTS % dict(attempt=self._attempt)
    try:
      for gce_test in self._run.config.gce_tests:
        logging.info('Running GCE test %s.', gce_test.test_type)
        if gce_test.test_type == constants.GCE_SUITE_TEST_TYPE:
          per_test_results_dir = os.path.join(test_results_root,
                                              gce_test.test_suite)
        else:
          per_test_results_dir = os.path.join(test_results_root,
                                              gce_test.test_type)
        with cgroups.SimpleContainChildren('GCETest'):
          r = ' Reached GCETestStage test run timeout.'
          with timeout_util.Timeout(self.TEST_TIMEOUT, reason_message=r):
            self._RunTest(gce_test, per_test_results_dir)

    except Exception:
      logging.error(_ERROR_MSG % dict(test_name='GCETests',
                                      test_results=test_basename))
      raise
    finally:
      self._ArchiveTestResults(test_results_root, test_basename)


class MoblabVMTestStage(generic_stages.BoardSpecificBuilderStage,
                        generic_stages.ArchivingStageMixin):
  """Run autotests against a moblab vm setup.

  This stage launches a MoblabVm setup -- a local running moblab of the image
  under test and another local VM of a stable DUT connected to it -- and then
  runs some autotest tests against it.
  """

  option_name = 'tests'
  config_name = 'moblab_vm_tests'

  # This includes the time we expect to take to prepare and run the tests. It
  # excludes the time required to archive the results at the end.
  _PERFORM_TIMEOUT_S = 60 * 60

  def __str__(self):
    return type(self).__name__

  def PerformStage(self):
    test_root_in_chroot = commands.CreateTestRoot(self._build_root)
    test_root = path_util.FromChrootPath(test_root_in_chroot)
    results_dir = os.path.join(test_root, 'results')
    work_dir = os.path.join(test_root, 'workdir')
    osutils.SafeMakedirsNonRoot(results_dir)
    osutils.SafeMakedirsNonRoot(work_dir)

    try:
      r = ' reached %s test run timeout.' % self
      with timeout_util.Timeout(self._PERFORM_TIMEOUT_S, reason_message=r):
        self._PerformStage(work_dir, results_dir)
    except:
      logging.error(_ERROR_MSG % dict(test_name='MoblabVMTest',
                                      test_results='directory'))
      raise
    finally:
      self._ArchiveTestResults(results_dir)

  def _PerformStage(self, workdir, results_dir):
    """Actually performs this stage.

    Args:
      workdir: The workspace directory to use for all temporary files.
      results_dir: The directory to use to drop test results into.
    """
    dut_target_image = self._SubDutTargetImage(),
    osutils.SafeMakedirsNonRoot(self._Workspace(workdir))
    vms = moblab_vm.MoblabVm(self._Workspace(workdir))
    try:
      vms.Create(self.GetImageDirSymlink(), self.GetImageDirSymlink())
      vms.Start()
      RunMoblabTests(
          self._current_board,
          vms.moblab_ssh_port,
          dut_target_image,
          results_dir,
      )
      vms.Stop()
    except:
      # Ignore errors while arhiving images, but re-raise the original error.
      try:
        vms.Stop()
        self._ArchiveMoblabVMWorkspace(self._Workspace(workdir))
      except Exception as e:
        logging.error('Failed to archive VM images after test failure: %s', e)
      raise
    finally:
      vms.Destroy()

  def _Workspace(self, workdir):
    return os.path.join(workdir, 'workspace')

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def _ArchiveMoblabVMWorkspace(self, workspace):
    """Try to find the VM files used during testing and archive them.

    Args:
      workspace: Path to a directory used as moblabvm workspace.
    """
    tarball_relpath = 'workspace.tar.bz2'
    tarball_path = os.path.join(self.archive_path, tarball_relpath)
    cros_build_lib.CreateTarball(tarball_path, workspace,
                                 compression=cros_build_lib.COMP_BZIP2)
    self._Upload(tarball_relpath, 'workspace')

  @failures_lib.SetFailureType(failures_lib.InfrastructureFailure)
  def _ArchiveTestResults(self, results_dir):
    """Try to find the results dropped during testing and archive them.

    Args:
      results_dir: Path to a directory used for creating result files.
    """
    results_reldir = 'results'
    cros_build_lib.SudoRunCommand(['chmod', '-R', 'a+rw', results_dir],
                                  print_cmd=False)
    archive_dir = os.path.join(self.archive_path, results_reldir)
    osutils.RmDir(archive_dir, ignore_missing=True)

    def _ShouldIgnore(dirname, file_list):
      # gsutil hangs on broken symlinks.
      return [x for x in file_list if os.path.islink(os.path.join(dirname, x))]

    shutil.copytree(results_dir, archive_dir, symlinks=False,
                    ignore=_ShouldIgnore)
    self._Upload(results_reldir, 'results')
    # TODO(pprabhu): List the failed tests with links for easy access like
    # VMTest does. We can't simly use ListTests because the result directory
    # looks different from when ctest is used.

  def _SubDutTargetImage(self):
    """Return a "good" image of the DUT VM board from GS.

    This image will be used to provision the DUT VM. The two requirements here
    are:
    - The image should be good enough to pass the tests moblab runs against it.
      (mostly, should pass provision, and boot otherwise consistently).
    - The image should exist in GS, for the provision flow to work.
    """
    # TODO(pprabhu) This hard-coded version information corresponds to a
    # manually vetted recent build. This will stop working once R66 goes to
    # stable (~ July 2018). This should be replaced with a canary channel image
    # for the same board before that.
    return 'moblab-generic-vm-paladin/R65-10178.0.0-rc1'

  def _Upload(self, path, prefix):
    """Upload |path| to GS and print a link to it on the log."""
    logging.info('Uploading artifact %s to Google Storage...', path)
    with self.ArtifactUploader(archive=False, strict=False) as queue:
      queue.put([path])
    self.PrintDownloadLink(path, '%s: ' % prefix)



def ListTests(results_path, show_failed=True, show_passed=True):
  """Returns a list of tests.

  Parse the test report logs from autotest to find tests.

  Args:
    results_path: Path to the directory of test results.
    show_failed: Return failed tests.
    show_passed: Return passed tests.

  Returns:
    A lists of (test_name, relative/path/to/tests)
  """
  # TODO: we don't have to parse the log to find tests once
  # crbug.com/350520 is fixed.
  reports = []
  for path, _, filenames in os.walk(results_path):
    reports.extend([os.path.join(path, x) for x in filenames
                    if x == _TEST_REPORT_FILENAME])

  tests = []
  processed_tests = []
  match_pattern = []
  if show_failed:
    match_pattern.append(_TEST_FAILED)
  if show_passed:
    match_pattern.append(_TEST_PASSED)

  for report in reports:
    logging.info('Parsing test report %s', report)
    # Format used in the report:
    #   /path/to/base/dir/test_harness/all/SimpleTestUpdateAndVerify/ \
    #     2_autotest_tests/results-01-security_OpenSSLBlacklist [  FAILED  ]
    #   /path/to/base/dir/test_harness/all/SimpleTestUpdateAndVerify/ \
    #     2_autotest_tests/results-01-security_OpenSSLBlacklist/ \
    #     security_OpenBlacklist [  FAILED  ]
    with open(report) as f:
      folder_re = re.compile(r'([\./\w-]*)\s*\[\s*(\S+?)\s*\]')
      test_name_re = re.compile(r'results-[\d]+?-([\.\w_]*)')
      for line in f:
        r = folder_re.search(line)
        if r and r.group(2) in match_pattern:
          file_path = r.group(1)
          match = test_name_re.search(file_path)
          if match:
            test_name = match.group(1)
          else:
            # If no match is found (due to format change or other
            # reasons), simply use the last component of file_path.
            test_name = os.path.basename(file_path)

          # A test may have subtests. We don't want to list all subtests.
          if test_name not in processed_tests:
            base_dirname = os.path.basename(results_path)
            # Get the relative path from the test_results directory. Note
            # that file_path is a chroot path, while results_path is a
            # non-chroot path, so we cannot use os.path.relpath directly.
            rel_path = file_path.split(base_dirname)[1].lstrip(os.path.sep)
            tests.append((test_name, rel_path))
            processed_tests.append(test_name)
  return tests


def GetTestResultsDir(buildroot, test_results_dir):
  """Returns the test results directory located in chroot.

  Args:
    buildroot: Root directory where build occurs.
    test_results_dir: Path from buildroot/chroot to find test results.
      This must a subdir of /tmp.
  """
  test_results_dir = test_results_dir.lstrip('/')
  return os.path.join(buildroot, constants.DEFAULT_CHROOT_DIR, test_results_dir)


def ArchiveTestResults(results_path, archive_dir):
  """Archives the test results to |archive_dir|.

  Args:
    results_path: Path to test results.
    archive_dir: Local directory to archive to.
  """
  cros_build_lib.SudoRunCommand(['chmod', '-R', 'a+rw', results_path],
                                print_cmd=False)
  if os.path.exists(archive_dir):
    osutils.RmDir(archive_dir)

  def _ShouldIgnore(dirname, file_list):
    # Note: We exclude VM disk and memory images. Instead, they are
    # archived via ArchiveVMFiles. Also skip any symlinks. gsutil
    # hangs on broken symlinks.
    return [x for x in file_list if
            x.startswith(constants.VM_DISK_PREFIX) or
            x.startswith(constants.VM_MEM_PREFIX) or
            os.path.islink(os.path.join(dirname, x))]

  shutil.copytree(results_path, archive_dir, symlinks=False,
                  ignore=_ShouldIgnore)


def ArchiveVMFiles(buildroot, test_results_dir, archive_path):
  """Archives the VM memory and disk images into tarballs.

  There may be multiple tests (e.g. SimpleTestUpdate and
  SimpleTestUpdateAndVerify), and multiple files for each test (one
  for the VM disk, and one for the VM memory). We create a separate
  tar file for each of these files, so that each can be downloaded
  independently.

  Args:
    buildroot: Build root directory.
    test_results_dir: Path from buildroot/chroot to find test results.
      This must a subdir of /tmp.
    archive_path: Directory the tarballs should be written to.

  Returns:
    The paths to the tarballs.
  """
  images_dir = os.path.join(buildroot, 'chroot', test_results_dir.lstrip('/'))
  images = []
  for path, _, filenames in os.walk(images_dir):
    images.extend([os.path.join(path, filename) for filename in
                   fnmatch.filter(filenames, constants.VM_DISK_PREFIX + '*')])
    images.extend([os.path.join(path, filename) for filename in
                   fnmatch.filter(filenames, constants.VM_MEM_PREFIX + '*')])

  tar_files = []
  for image_path in images:
    image_rel_path = os.path.relpath(image_path, images_dir)
    image_parent_dir = os.path.dirname(image_path)
    image_file = os.path.basename(image_path)
    tarball_path = os.path.join(archive_path,
                                "%s.tar" % image_rel_path.replace('/', '_'))
    # Note that tar will chdir to |image_parent_dir|, so that |image_file|
    # is at the top-level of the tar file.
    cros_build_lib.CreateTarball(tarball_path,
                                 image_parent_dir,
                                 compression=cros_build_lib.COMP_BZIP2,
                                 inputs=[image_file])
    tar_files.append(tarball_path)
  return tar_files


def RunCrosVMTest(board, image_dir):
  """Runs cros_vm_test script to verify cros commands work."""
  image_path = os.path.join(image_dir, constants.TEST_IMAGE_BIN)
  test = cros_vm_test.CrosVMTest(board, image_path)
  test.Run()


def RunDevModeTest(buildroot, board, image_dir):
  """Runs the dev mode testing script to verify dev-mode scripts work."""
  crostestutils = os.path.join(buildroot, 'src', 'platform', 'crostestutils')
  image_path = os.path.join(image_dir, constants.TEST_IMAGE_BIN)
  test_script = 'devmode-test/devinstall_test.py'
  cmd = [os.path.join(crostestutils, test_script), '--verbose', board,
         image_path]
  cros_build_lib.RunCommand(cmd)


def RunTestSuite(buildroot, board, image_path, results_dir, test_config,
                 whitelist_chrome_crashes, archive_dir, ssh_private_key=None):
  """Runs the test harness suite."""
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  osutils.RmDir(results_dir_in_chroot, ignore_missing=True)

  test_type = test_config.test_type
  cwd = os.path.join(buildroot, 'src', 'scripts')
  dut_type = 'gce' if test_type == constants.GCE_SUITE_TEST_TYPE else 'vm'

  cmd = ['bin/ctest',
         '--board=%s' % board,
         '--type=%s' % dut_type,
         '--no_graphics',
         '--verbose',
         '--target_image=%s' % image_path,
         '--test_results_root=%s' % results_dir_in_chroot
        ]

  if test_type not in constants.VALID_VM_TEST_TYPES:
    raise AssertionError('Unrecognized test type %r' % test_type)

  if test_type == constants.FULL_AU_TEST_TYPE:
    cmd.append('--archive_dir=%s' % archive_dir)
  elif test_type in [constants.VM_SUITE_TEST_TYPE,
                     constants.GCE_SUITE_TEST_TYPE]:
    cmd.append('--only_verify')
    cmd.append('--suite=%s' % test_config.test_suite)
  else:
    cmd.append('--quick_update')

  if whitelist_chrome_crashes:
    cmd.append('--whitelist_chrome_crashes')

  if ssh_private_key is not None:
    cmd.append('--ssh_private_key=%s' % ssh_private_key)

  # Give tests 10 minutes to clean up before shutting down.
  result = cros_build_lib.RunCommand(cmd, cwd=cwd, error_code_ok=True,
                                     kill_timeout=10 * 60)
  if result.returncode:
    if os.path.exists(results_dir_in_chroot):
      error = '%s exited with code %d' % (' '.join(cmd), result.returncode)
      with open(results_dir_in_chroot + '/failed_test_command', 'w') as failed:
        failed.write(error)

    raise failures_lib.TestFailure(
        '** VMTests failed with code %d **' % result.returncode)


def RunMoblabTests(moblab_board, moblab_ip, dut_target_image, results_dir):
  """Run the moblab test suite against a running moblab_vm setup.

  Args:
    moblab_board: Board name of the moblab DUT.
    moblab_ip: IP address of moblab VM.
    dut_target_image: Image string to provision onto the DUT VM. This image must
        exist on GS so that the provision flow can download and install it on
        the DUT VM.
    results_dir: Directory to drop results into.
  """
  test_args = [
      # moblab in VM takes longer to bring up all upstart services on first
      # boot than on physical machines.
      'services_init_timeout_m=10',
      'target_build="%s"' % dut_target_image,
  ]
  cros_build_lib.RunCommand(
      [
          'test_that',
          '--no-quickmerge',
          '-b', moblab_board,
          '--results_dir', path_util.ToChrootPath(results_dir),
          'localhost:%s' % moblab_ip, 'moblab_DummyServerSuite',
          '--args', ' '.join(test_args),
      ],
      enter_chroot=True,
  )
