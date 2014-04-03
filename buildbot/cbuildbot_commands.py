# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various individual commands a builder can run."""

from datetime import datetime
import fnmatch
import glob
import logging
import multiprocessing
import os
import re
import shutil
import tempfile
import time

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import portage_utilities
from chromite.cros.tests import cros_vm_test
from chromite.lib import cros_build_lib
from chromite.lib import gclient
from chromite.lib import git
from chromite.lib import gs
from chromite.lib import locking
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import retry_util
from chromite.lib import timeout_util
from chromite.scripts import pushimage
from chromite.scripts import upload_symbols


_PACKAGE_FILE = '%(buildroot)s/src/scripts/cbuildbot_package.list'
CHROME_KEYWORDS_FILE = ('/build/%(board)s/etc/portage/package.keywords/chrome')
_PREFLIGHT_BINHOST = 'PREFLIGHT_BINHOST'
_CHROME_BINHOST = 'CHROME_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
_FACTORY_SHIM = 'factory_shim'
_FULL_BINHOST = 'FULL_BINHOST'
_PRIVATE_BINHOST_CONF_DIR = ('src/private-overlays/chromeos-partner-overlay/'
                             'chromeos/binhost')
_BINHOST_PACKAGE_FILE = ('/usr/share/dev-install/portage/make.profile/'
                         'package.installable')
_AUTOTEST_RPC_CLIENT = ('/b/build_internal/scripts/slave-internal/autotest_rpc/'
                        'autotest_rpc_client.py')
_AUTOTEST_RPC_HOSTNAME = 'master2'
_LOCAL_BUILD_FLAGS = ['--nousepkg', '--reuse_pkgs_from_local_boards']
UPLOADED_LIST_FILENAME = 'UPLOADED'
# For sorting through VM test results.
_TEST_REPORT_FILENAME = 'test_report.log'
_TEST_PASSED = 'PASSED'
_TEST_FAILED = 'FAILED'


class TestFailure(results_lib.StepFailure):
  """Raised if a test stage (e.g. VMTest) fails."""

# =========================== Command Helpers =================================


def _RunBuildScript(buildroot, cmd, chromite_cmd=False, possibly_flaky=False,
                    **kwargs):
  """Run a build script, wrapping exceptions as needed.

  This wraps RunCommand(cmd, cwd=buildroot, **kwargs), adding extra logic to
  help determine the cause of command failures.
    - If a package fails to build, a PackageBuildFailure exception is thrown,
      which lists exactly which packages failed to build.
    - If the command fails for a different reason, a BuildScriptFailure
      exception is thrown.

  We detect what packages failed to build by creating a temporary status file,
  and passing that status file to parallel_emerge via the
  PARALLEL_EMERGE_STATUS_FILE variable.

  Args:
    buildroot: The root of the build directory.
    cmd: The command to run.
    chromite_cmd: Whether the command should be evaluated relative to the
      chromite/bin subdir of the |buildroot|.
    possibly_flaky: Whether this failure is likely to fail occasionally due
      to flakiness (e.g. network flakiness).
    kwargs: Optional args passed to RunCommand; see RunCommand for specifics.
  """
  assert not kwargs.get('shell', False), 'Cannot execute shell commands'
  kwargs.setdefault('cwd', buildroot)
  enter_chroot = kwargs.get('enter_chroot', False)

  if chromite_cmd:
    cmd = cmd[:]
    if enter_chroot:
      cmd[0] = git.ReinterpretPathForChroot(
          os.path.join(constants.CHROMITE_BIN_SUBDIR, cmd[0]))
    else:
      cmd[0] = os.path.join(buildroot, constants.CHROMITE_BIN_SUBDIR, cmd[0])

  # If we are entering the chroot, create status file for tracking what
  # packages failed to build.
  chroot_tmp = os.path.join(buildroot, 'chroot', 'tmp')
  status_file = None
  with cros_build_lib.ContextManagerStack() as stack:
    if enter_chroot and os.path.exists(chroot_tmp):
      kwargs['extra_env'] = (kwargs.get('extra_env') or {}).copy()
      status_file = stack.Add(tempfile.NamedTemporaryFile, dir=chroot_tmp)
      kwargs['extra_env']['PARALLEL_EMERGE_STATUS_FILE'] = \
          git.ReinterpretPathForChroot(status_file.name)
    try:
      return cros_build_lib.RunCommand(cmd, **kwargs)
    except cros_build_lib.RunCommandError as ex:
      # Print the original exception.
      cros_build_lib.Error('\n%s', ex)

      # Check whether a specific package failed. If so, wrap the exception
      # appropriately. These failures are usually caused by a recent CL, so we
      # don't ever treat these failures as flaky.
      if status_file is not None:
        status_file.seek(0)
        failed_packages = status_file.read().split()
        if failed_packages:
          raise results_lib.PackageBuildFailure(ex, cmd[0], failed_packages)

      # Looks like a generic failure. Raise a BuildScriptFailure.
      raise results_lib.BuildScriptFailure(ex, cmd[0],
                                           possibly_flaky=possibly_flaky)


def GetInput(prompt):
  """Helper function to grab input from a user.   Makes testing easier."""
  return raw_input(prompt)


def ValidateClobber(buildroot):
  """Do due diligence if user wants to clobber buildroot.

  Args:
    buildroot: buildroot that's potentially clobbered.

  Returns:
    True if the clobber is ok.
  """
  cwd = os.path.dirname(os.path.realpath(__file__))
  if cwd.startswith(buildroot):
    cros_build_lib.Die('You are trying to clobber this chromite checkout!')

  if buildroot == '/':
    cros_build_lib.Die('Refusing to clobber your system!')

  if os.path.exists(buildroot):
    return cros_build_lib.BooleanPrompt(default=False)
  return True


# =========================== Main Commands ===================================


def BuildRootGitCleanup(buildroot):
  """Put buildroot onto manifest branch. Delete branches created on last run.

  Args:
    buildroot: buildroot to clean up.
  """
  lock_path = os.path.join(buildroot, '.clean_lock')
  deleted_objdirs = multiprocessing.Event()

  def RunCleanupCommands(project, cwd):
    with locking.FileLock(lock_path, verbose=False).read_lock() as lock:
      # Calculate where the git repository is stored.
      relpath = os.path.relpath(cwd, buildroot)
      projects_dir = os.path.join(buildroot, '.repo', 'projects')
      project_objects_dir = os.path.join(buildroot, '.repo', 'project-objects')
      repo_git_store = '%s.git' % os.path.join(projects_dir, relpath)
      repo_obj_store = '%s.git' % os.path.join(project_objects_dir, project)

      try:
        if os.path.isdir(cwd):
          git.CleanAndDetachHead(cwd)
          git.GarbageCollection(cwd)
      except cros_build_lib.RunCommandError as e:
        result = e.result
        cros_build_lib.PrintBuildbotStepWarnings()
        logging.warn('\n%s', result.error)

        # If there's no repository corruption, just delete the index.
        corrupted = git.IsGitRepositoryCorrupted(cwd)
        lock.write_lock()
        logging.warn('Deleting %s because %s failed', cwd, result.cmd)
        osutils.RmDir(cwd, ignore_missing=True)
        if corrupted:
          # Looks like the object dir is corrupted. Delete the whole repository.
          deleted_objdirs.set()
          for store in (repo_git_store, repo_obj_store):
            logging.warn('Deleting %s as well', store)
            osutils.RmDir(store, ignore_missing=True)

      # Delete all branches created by cbuildbot.
      if os.path.isdir(repo_git_store):
        cmd = ['branch', '-D'] + list(constants.CREATED_BRANCHES)
        git.RunGit(repo_git_store, cmd, error_code_ok=True)

  # Cleanup all of the directories.
  dirs = [[attrs['name'], os.path.join(buildroot, attrs['path'])] for attrs in
          git.ManifestCheckout.Cached(buildroot).ListCheckouts()]
  parallel.RunTasksInProcessPool(RunCleanupCommands, dirs)

  # repo shares git object directories amongst multiple project paths. If the
  # first pass deleted an object dir for a project path, then other repositories
  # (project paths) of that same project may now be broken. Do a second pass to
  # clean them up as well.
  if deleted_objdirs.is_set():
    parallel.RunTasksInProcessPool(RunCleanupCommands, dirs)


def CleanUpMountPoints(buildroot):
  """Cleans up any stale mount points from previous runs."""
  # Scrape it from /proc/mounts since it's easily accessible;
  # additionally, unmount in reverse order of what's listed there
  # rather than trying a reverse sorting; it's possible for
  # mount /z /foon
  # mount /foon/blah -o loop /a
  # which reverse sorting cannot handle.
  buildroot = os.path.realpath(buildroot).rstrip('/') + '/'
  mounts = []
  with open("/proc/mounts", 'rt') as f:
    for line in f:
      path = line.split()[1]
      if path.startswith(buildroot):
        mounts.append(path)

  for mount_pt in reversed(mounts):
    osutils.UmountDir(mount_pt, lazy=True, cleanup=False)


def WipeOldOutput(buildroot):
  """Wipes out build output directory.

  Args:
    buildroot: Root directory where build occurs.
    board: Delete image directories for this board name.
  """
  image_dir = os.path.join(buildroot, 'src', 'build', 'images')
  osutils.RmDir(image_dir, ignore_missing=True, sudo=True)


def MakeChroot(buildroot, replace, use_sdk, chrome_root=None, extra_env=None):
  """Wrapper around make_chroot."""
  cmd = ['cros_sdk', '--buildbot-log-version']
  cmd.append('--create' if use_sdk else '--bootstrap')

  if replace:
    cmd.append('--replace')

  if chrome_root:
    cmd.append('--chrome_root=%s' % chrome_root)

  _RunBuildScript(buildroot, cmd, extra_env=extra_env)


def RunChrootUpgradeHooks(buildroot, chrome_root=None):
  """Run the chroot upgrade hooks in the chroot."""
  chroot_args = []
  if chrome_root:
    chroot_args.append('--chrome_root=%s' % chrome_root)

  _RunBuildScript(buildroot, ['./run_chroot_version_hooks'],
                  enter_chroot=True, chroot_args=chroot_args)


def RefreshPackageStatus(buildroot, boards, debug):
  """Wrapper around refresh_package_status"""
  # First run check_gdata_token to validate or refresh auth token.
  cmd = ['check_gdata_token']
  _RunBuildScript(buildroot, cmd, chromite_cmd=True)

  # Prepare refresh_package_status command to update the package spreadsheet.
  cmd = ['refresh_package_status']

  # Skip the host board if present.
  board = ':'.join([b for b in boards if b != 'amd64-host'])
  cmd.append('--board=%s' % board)

  # Upload to the test spreadsheet only when in debug mode.
  if debug:
    cmd.append('--test-spreadsheet')

  # Actually run prepared refresh_package_status command.
  _RunBuildScript(buildroot, cmd, chromite_cmd=True, enter_chroot=True)

  # Disabling the auto-filing of Tracker issues for now - crbug.com/334260.
  #SyncPackageStatus(buildroot, debug)


def SyncPackageStatus(buildroot, debug):
  """Wrapper around sync_package_status."""
  # Run sync_package_status to create Tracker issues for outdated
  # packages.  At the moment, this runs only for groups that have opted in.
  basecmd = ['sync_package_status']
  if debug:
    basecmd.extend(['--pretend', '--test-spreadsheet'])

  cmdargslist = [['--team=build'],
                 ['--team=kernel', '--default-owner=arscott'],
                 ]

  for cmdargs in cmdargslist:
    cmd = basecmd + cmdargs
    _RunBuildScript(buildroot, cmd, chromite_cmd=True, enter_chroot=True)


def SetSharedUserPassword(buildroot, password):
  """Wrapper around set_shared_user_password.sh"""
  if password is not None:
    cmd = ['./set_shared_user_password.sh', password]
    _RunBuildScript(buildroot, cmd, enter_chroot=True)
  else:
    passwd_file = os.path.join(buildroot, 'chroot/etc/shared_user_passwd.txt')
    osutils.SafeUnlink(passwd_file, sudo=True)


def SetupBoard(buildroot, board, usepkg, chrome_binhost_only=False,
               extra_env=None, force=False, profile=None, chroot_upgrade=True):
  """Wrapper around setup_board.

  Args:
    buildroot: The buildroot of the current build.
    board: The board to set up.
    usepkg: Whether to use binary packages when setting up the board.
    chrome_binhost_only: If set, only use binary packages on the board for
      Chrome itself.
    extra_env: A dictionary of environmental variables to set during generation.
    force: Whether to remove the board prior to setting it up.
    profile: The profile to use with this board.
    chroot_upgrade: Whether to update the chroot. If the chroot is already up to
      date, you can specify chroot_upgrade=False.
  """
  cmd = ['./setup_board', '--board=%s' % board,
         '--accept_licenses=@CHROMEOS']

  # This isn't the greatest thing, but emerge's dependency calculation
  # isn't the speediest thing, so let callers skip this step when they
  # know the system is up-to-date already.
  if not chroot_upgrade:
    cmd.append('--skip_chroot_upgrade')

  if profile:
    cmd.append('--profile=%s' % profile)

  if not usepkg:
    cmd.extend(_LOCAL_BUILD_FLAGS)

  if chrome_binhost_only:
    cmd.append('--chrome_binhost_only')

  if force:
    cmd.append('--force')

  _RunBuildScript(buildroot, cmd, extra_env=extra_env, enter_chroot=True)


def Build(buildroot, board, build_autotest, usepkg, chrome_binhost_only,
          packages=(), skip_chroot_upgrade=True, noworkon=False,
          extra_env=None, chrome_root=None):
  """Wrapper around build_packages.

  Args:
    buildroot: The buildroot of the current build.
    board: The board to set up.
    build_autotest: Whether to build autotest-related packages.
    usepkg: Whether to use binary packages.
    chrome_binhost_only: If set, only use binary packages on the board for
      Chrome itself.
    packages: Tuple of specific packages we want to build. If empty,
      build_packages will calculate a list of packages automatically.
    skip_chroot_upgrade: Whether to skip the chroot update. If the chroot is
      not yet up to date, you should specify skip_chroot_upgrade=False.
    noworkon: If set, don't force-build workon packages.
    extra_env: A dictionary of environmental variables to set during generation.
    chrome_root: The directory where chrome is stored.
  """
  cmd = ['./build_packages', '--board=%s' % board,
         '--accept_licenses=@CHROMEOS']

  if not build_autotest:
    cmd.append('--nowithautotest')

  if skip_chroot_upgrade:
    cmd.append('--skip_chroot_upgrade')

  if not usepkg:
    cmd.extend(_LOCAL_BUILD_FLAGS)

  if chrome_binhost_only:
    cmd.append('--chrome_binhost_only')

  if noworkon:
    cmd.append('--noworkon')

  chroot_args = []
  if chrome_root:
    chroot_args.append('--chrome_root=%s' % chrome_root)

  cmd.extend(packages)
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, chroot_args=chroot_args,
                  enter_chroot=True)


def BuildImage(buildroot, board, images_to_build, version=None,
               rootfs_verification=True, extra_env=None, disk_layout=None):

  # Default to base if images_to_build is passed empty.
  if not images_to_build:
    images_to_build = ['base']

  version_str = '--version=%s' % (version or '')

  cmd = ['./build_image', '--board=%s' % board, '--replace', version_str]

  if not rootfs_verification:
    cmd += ['--noenable_rootfs_verification']

  if disk_layout:
    cmd += ['--disk_layout=%s' % disk_layout]

  cmd += images_to_build

  _RunBuildScript(buildroot, cmd, extra_env=extra_env, enter_chroot=True)


def GenerateAuZip(buildroot, image_dir, extra_env=None):
  """Run the script which generates au-generator.zip.

  Args:
    buildroot: The buildroot of the current build.
    image_dir: The directory in which to store au-generator.zip.
    extra_env: A dictionary of environmental variables to set during generation.

  Raises:
    results_lib.BuildScriptFailure if the called script fails.
  """
  chroot_image_dir = git.ReinterpretPathForChroot(image_dir)
  cmd = ['./build_library/generate_au_zip.py', '-o', chroot_image_dir]
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, enter_chroot=True)


def TestAuZip(buildroot, image_dir, extra_env=None):
  """Run the script which validates an au-generator.zip.

  Args:
    buildroot: The buildroot of the current build.
    image_dir: The directory in which to find au-generator.zip.
    extra_env: A dictionary of environmental variables to set during generation.

  Raises:
    results_lib.BuildScriptFailure if the test script fails.
  """
  cmd = ['./build_library/test_au_zip.py', '-o', image_dir]
  _RunBuildScript(buildroot, cmd,
                  cwd=constants.CROSUTILS_DIR,
                  extra_env=extra_env)


def BuildVMImageForTesting(buildroot, board, extra_env=None, disk_layout=None):
  cmd = ['./image_to_vm.sh', '--board=%s' % board, '--test_image']
  if disk_layout:
    cmd += ['--disk_layout=%s' % disk_layout]
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, enter_chroot=True)


def RunSignerTests(buildroot, board):
  cmd = ['./security_test_image', '--board=%s' % board]
  _RunBuildScript(buildroot, cmd, enter_chroot=True)


def RunUnitTests(buildroot, board, full, blacklist=None, extra_env=None):
  cmd = ['cros_run_unit_tests', '--board=%s' % board]

  # If we aren't running ALL tests, then restrict to just the packages
  #   uprev noticed were changed.
  if not full:
    package_file = _PACKAGE_FILE % {'buildroot': buildroot}
    cmd += ['--package_file=%s' % git.ReinterpretPathForChroot(package_file)]

  if blacklist:
    cmd += ['--blacklist_packages=%s' % ' '.join(blacklist)]

  _RunBuildScript(buildroot, cmd, enter_chroot=True, extra_env=extra_env or {})


def RunTestSuite(buildroot, board, image_dir, results_dir, test_type,
                 whitelist_chrome_crashes, archive_dir):
  """Runs the test harness suite."""
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  osutils.RmDir(results_dir_in_chroot, ignore_missing=True)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')

  cmd = ['bin/ctest',
         '--board=%s' % board,
         '--type=vm',
         '--no_graphics',
         '--target_image=%s' % image_path,
         '--test_results_root=%s' % results_dir_in_chroot
        ]

  if test_type not in constants.VALID_VM_TEST_TYPES:
    raise AssertionError('Unrecognized test type %r' % test_type)

  if test_type == constants.FULL_AU_TEST_TYPE:
    cmd.append('--archive_dir=%s' % archive_dir)
  else:
    cmd.append('--quick')
    if test_type == constants.SMOKE_SUITE_TEST_TYPE:
      cmd.append('--only_verify')
      cmd.append('--suite=smoke')
    elif test_type == constants.TELEMETRY_SUITE_TEST_TYPE:
      cmd.append('--suite=telemetry_unit')

  if whitelist_chrome_crashes:
    cmd.append('--whitelist_chrome_crashes')

  result = cros_build_lib.RunCommand(cmd, cwd=cwd, error_code_ok=True)
  if result.returncode:
    if os.path.exists(results_dir_in_chroot):
      error = '%s exited with code %d' % (' '.join(cmd), result.returncode)
      with open(results_dir_in_chroot + '/failed_test_command', 'w') as failed:
        failed.write(error)
    # We already retry VMTest inline, so we can assume that failures in VMTest
    # are not flaky.
    raise TestFailure('** VMTests failed with code %d **'
                      % result.returncode, possibly_flaky=False)


def RunDevModeTest(buildroot, board, image_dir):
  """Runs the dev mode testing script to verify dev-mode scripts work."""
  crostestutils = os.path.join(buildroot, 'src', 'platform', 'crostestutils')
  image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')
  test_script = 'devmode-test/devinstall_test.py'
  cmd = [os.path.join(crostestutils, test_script), '--verbose', board,
         image_path]
  cros_build_lib.RunCommand(cmd)


def RunCrosVMTest(board, image_dir):
  """Runs cros_vm_test script to verify cros flash/deploy works."""
  image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')
  test = cros_vm_test.CrosCommandTest(board, image_path)
  test.Run()


def ListFailedTests(results_path):
  """Returns a list of failed tests.

  Parse the test report logs from autotest to find failed tests.

  Args:
    results_path: Path to the directory of test results.

  Returns:
    A lists of (test_name, relative/path/to/failed/tests)
  """
  # TODO: we don't have to parse the log to find failed tests once
  # crbug.com/350520 is fixed.
  reports = []
  for path, _, filenames in os.walk(results_path):
    reports.extend([os.path.join(path, x) for x in filenames
                    if x == _TEST_REPORT_FILENAME])

  failed_tests = []
  processed_tests = []
  for report in reports:
    # Format used in the report:
    #   /path/to/base/dir/test_harness/all/SimpleTestUpdateAndVerify/ \
    #     2_autotest_tests/results-01-security_OpenSSLBlacklist [  FAILED  ]
    #   /path/to/base/dir/test_harness/all/SimpleTestUpdateAndVerify/ \
    #     2_autotest_tests/results-01-security_OpenSSLBlacklist/ \
    #     security_OpenBlacklist [  FAILED  ]
    with open(report) as f:
      failed_re = re.compile(r'([\./\w-]*)\s*\[\s*(\S+?)\s*\]')
      test_name_re = re.compile(r'results-[\d]+?-([\.\w_]*)')
      for line in f:
        r = failed_re.search(line)
        if r and r.group(2) == _TEST_FAILED:
          # Process only failed tests.
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
            failed_tests.append((test_name, rel_path))
            processed_tests.append(test_name)

    return failed_tests


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


def BuildAndArchiveTestResultsTarball(src_dir, buildroot):
  """Create a compressed tarball of test results.

  Args:
    src_dir: The directory containing the test results.
    buildroot: Build root directory.

  Returns:
    The name of the tarball.
  """
  target = '%s.tgz' % src_dir.rstrip(os.path.sep)
  chroot = os.path.join(buildroot, constants.DEFAULT_CHROOT_DIR)
  cros_build_lib.CreateTarball(
      target, src_dir, compression=cros_build_lib.COMP_GZIP,
      chroot=chroot)
  return os.path.basename(target)


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


def RunHWTestSuite(build, suite, board, pool=None, num=None, file_bugs=None,
                   wait_for_results=None, priority=None, timeout_mins=None,
                   debug=True):
  """Run the test suite in the Autotest lab.

  Args:
    build: The build is described as the bot_id and the build version.
      e.g. x86-mario-release/R18-1655.0.0-a1-b1584.
    suite: Name of the Autotest suite.
    board: The board the test suite should be scheduled against.
    pool: The pool of machines we should use to run the hw tests on.
    num: Maximum number of devices to use when scheduling tests in the
         hardware test lab.
    file_bugs: File bugs on test failures for this suite run.
    wait_for_results: If True, wait for autotest results before returning.
    priority: Priority of this suite run.
    timeout_mins: Timeout in minutes for the suite job and its sub-jobs.
    debug: Whether we are in debug mode.
  """
  # TODO(scottz): RPC client option names are misnomers crosbug.com/26445.
  cmd = [_AUTOTEST_RPC_CLIENT,
         _AUTOTEST_RPC_HOSTNAME,
         'RunSuite',
         '--build', build,
         '--suite_name', suite,
         '--board', board]

  # Add optional arguments to command, if present.
  if pool is not None:
    cmd += ['--pool', pool]

  if num is not None:
    cmd += ['--num', str(num)]

  if file_bugs is not None:
    cmd += ['--file_bugs', str(file_bugs)]

  if wait_for_results is not None:
    cmd += ['--no_wait', str(not wait_for_results)]

  if priority is not None:
    cmd += ['--priority', priority]

  if timeout_mins is not None:
    cmd += ['--timeout_mins', str(timeout_mins)]

  if debug:
    cros_build_lib.Info('RunHWTestSuite would run: %s',
                        cros_build_lib.CmdToStr(cmd))
  else:
    result = cros_build_lib.RunCommand(cmd, error_code_ok=True)
    if result.returncode:
      raise TestFailure('** HWTests failed with code %d **'
                        % result.returncode, possibly_flaky=True)


def _GetAbortCQHWTestsURL(version, suite):
  """Get the URL where we should save state about the specified abort command.

  Args:
    version: The version of the current build. E.g. R18-1655.0.0-rc1
    suite: The suite argument that AbortCQHWTests was called with, if any.
  """
  url = '%s/hwtests-aborted/%s/suite=%s'
  return url % (constants.MANIFEST_VERSIONS_GS_URL, version, suite)


def AbortCQHWTests(version, debug, suite=''):
  """Abort the specified hardware tests on the commit queue.

  Args:
    version: The version of the current build. E.g. R18-1655.0.0-rc1
    debug: Whether we are in debug mode.
    suite: Name of the Autotest suite. If empty, abort all suites.
  """
  # Mark the substr/suite as aborted in Google Storage.
  ctx = gs.GSContext(dry_run=debug)
  ctx.Copy('-', _GetAbortCQHWTestsURL(version, suite), input='')

  # Abort all jobs for the given version, containing the '-paladin' suffix.
  # Example job id: link-paladin/R35-5542.0.0-rc1
  substr = '%s/%s' % (cbuildbot_config.CONFIG_TYPE_PALADIN, version)

  # Actually abort the build.
  cmd = [_AUTOTEST_RPC_CLIENT,
         _AUTOTEST_RPC_HOSTNAME,
         'AbortSuiteByName',
         '-i', substr,
         '-s', suite]
  if debug:
    cros_build_lib.Info('AbortCQHWTests would run: %s',
                        cros_build_lib.CmdToStr(cmd))
  else:
    try:
      cros_build_lib.RunCommand(cmd)
    except cros_build_lib.RunCommandError:
      cros_build_lib.Warning('AbortCQHWTests failed', exc_info=True)


def HaveCQHWTestsBeenAborted(version, suite=''):
  """Check in Google Storage whether the specified abort call was sent.

  This function will return True if the following call has occurred:
    AbortCQHWTests(version, debug=False, suite=suite)

  Args:
    version: The version of the current build. E.g. R18-1655.0.0-rc1
    suite: The suite argument that AbortCQHWTests was called with, if any.
  """
  return gs.GSContext().Exists(_GetAbortCQHWTestsURL(version, suite))


def GenerateStackTraces(buildroot, board, test_results_dir,
                        archive_dir, got_symbols):
  """Generates stack traces for logs in |gzipped_test_tarball|

  Args:
    buildroot: Root directory where build occurs.
    board: Name of the board being worked on.
    test_results_dir: Directory of the test results.
    archive_dir: Local directory for archiving.
    got_symbols: True if breakpad symbols have been generated.

  Returns:
    List of stack trace file names.
  """
  stack_trace_filenames = []
  asan_log_signaled = False

  board_path = cros_build_lib.GetSysroot(board=board)
  symbol_dir = os.path.join(board_path, 'usr', 'lib', 'debug', 'breakpad')
  for curr_dir, _subdirs, files in os.walk(test_results_dir):
    for curr_file in files:
      full_file_path = os.path.join(curr_dir, curr_file)
      processed_file_path = '%s.txt' % full_file_path

      # Distinguish whether the current file is a minidump or asan_log.
      if curr_file.endswith('.dmp'):
        # Skip crash files that were purposely generated or if
        # breakpad symbols are absent.
        if not got_symbols or curr_file.find('crasher_nobreakpad') == 0:
          continue
        # Precess the minidump from within chroot.
        minidump = git.ReinterpretPathForChroot(full_file_path)
        cwd = os.path.join(buildroot, 'src', 'scripts')
        cros_build_lib.RunCommand(
            ['minidump_stackwalk', minidump, symbol_dir], cwd=cwd,
            enter_chroot=True, error_code_ok=True, redirect_stderr=True,
            debug_level=logging.DEBUG, log_stdout_to_file=processed_file_path)
      # Process asan log.
      else:
        # Prepend '/chrome/$board' path to the stack trace in log.
        log_content = ''
        with open(full_file_path) as f:
          for line in f:
            # Stack frame line example to be matched here:
            #    #0 0x721d1831 (/opt/google/chrome/chrome+0xb837831)
            stackline_match = re.search(r'^ *#[0-9]* 0x.* \(', line)
            if stackline_match:
              frame_end = stackline_match.span()[1]
              line = line[:frame_end] + board_path + line[frame_end:]
            log_content += line
        # Symbolize and demangle it.
        raw = cros_build_lib.RunCommand(
            ['asan_symbolize.py'], input=log_content, enter_chroot=True,
            debug_level=logging.DEBUG, capture_output=True,
            extra_env={'LLVM_SYMBOLIZER_PATH' : '/usr/bin/llvm-symbolizer'})
        cros_build_lib.RunCommand(['c++filt'],
                                  input=raw.output, debug_level=logging.DEBUG,
                                  cwd=buildroot, redirect_stderr=True,
                                  log_stdout_to_file=processed_file_path)
        # Break the bot if asan_log found. This is because some asan
        # crashes may not fail any test so the bot stays green.
        # Ex: crbug.com/167497
        if not asan_log_signaled:
          asan_log_signaled = True
          cros_build_lib.Error(
              'Asan crash occurred. See asan_logs in Artifacts.')
          cros_build_lib.PrintBuildbotStepFailure()

      # Append the processed file to archive.
      filename = ArchiveFile(processed_file_path, archive_dir)
      stack_trace_filenames.append(filename)

  return stack_trace_filenames


def ArchiveFile(file_to_archive, archive_dir):
  """Archives the specified file.

  Args:
    file_to_archive: Full path to file to archive.
    archive_dir: Local directory for archiving.

  Returns:
    The base name of the archived file.
  """
  filename = os.path.basename(file_to_archive)
  if archive_dir:
    archived_file = os.path.join(archive_dir, filename)
    shutil.copy(file_to_archive, archived_file)
    os.chmod(archived_file, 0o644)

  return filename


def MarkChromeAsStable(buildroot,
                       tracking_branch,
                       chrome_rev,
                       boards,
                       chrome_version=None):
  """Returns the portage atom for the revved chrome ebuild - see man emerge."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  extra_env = None
  chroot_args = None

  command = ['../../chromite/bin/cros_mark_chrome_as_stable',
             '--tracking_branch=%s' % tracking_branch,
             '--boards=%s' % ':'.join(boards), ]
  if chrome_version:
    command.append('--force_revision=%s' % chrome_version)
    assert chrome_rev == constants.CHROME_REV_SPEC, (
        'Cannot rev non-spec with a chrome_version')

  portage_atom_string = cros_build_lib.RunCommand(
      command + [chrome_rev],
      cwd=cwd,
      redirect_stdout=True,
      enter_chroot=True,
      chroot_args=chroot_args,
      extra_env=extra_env).output.rstrip()
  chrome_atom = None
  if portage_atom_string:
    chrome_atom = portage_atom_string.splitlines()[-1].partition('=')[-1]
  if not chrome_atom:
    cros_build_lib.Info('Found nothing to rev.')
    return None

  for board in boards:
    # If we're using a version of Chrome other than the latest one, we need
    # to unmask it manually.
    if chrome_rev != constants.CHROME_REV_LATEST:
      keywords_file = CHROME_KEYWORDS_FILE % {'board': board}
      cros_build_lib.SudoRunCommand(
          ['mkdir', '-p', os.path.dirname(keywords_file)],
          enter_chroot=True, cwd=cwd)
      cros_build_lib.SudoRunCommand(
          ['tee', keywords_file], input='=%s\n' % chrome_atom,
          enter_chroot=True, cwd=cwd)

    # Sanity check: We should always be able to merge the version of
    # Chrome we just unmasked.
    result = cros_build_lib.RunCommand(
        ['emerge-%s' % board, '-p', '--quiet', '=%s' % chrome_atom],
        enter_chroot=True, error_code_ok=True, combine_stdout_stderr=True,
        capture_output=True)
    if result.returncode:
      cros_build_lib.PrintBuildbotStepWarnings()
      cros_build_lib.Warning('\n%s' % result.output)
      cros_build_lib.Warning('Cannot emerge-%s =%s\nIs Chrome pinned to an '
                             'older version?' % (board, chrome_atom))
      return None

  return chrome_atom


def CleanupChromeKeywordsFile(boards, buildroot):
  """Cleans chrome uprev artifact if it exists."""
  for board in boards:
    keywords_path_in_chroot = CHROME_KEYWORDS_FILE % {'board': board}
    keywords_file = '%s/chroot%s' % (buildroot, keywords_path_in_chroot)
    if os.path.exists(keywords_file):
      cros_build_lib.SudoRunCommand(['rm', '-f', keywords_file])


def UprevPackages(buildroot, boards, overlays, enter_chroot=True):
  """Uprevs non-browser chromium os packages that have changed."""
  drop_file = _PACKAGE_FILE % {'buildroot': buildroot}
  if enter_chroot:
    overlays = [git.ReinterpretPathForChroot(x) for x in overlays]
    drop_file = git.ReinterpretPathForChroot(drop_file)
  cmd = ['cros_mark_as_stable', '--all',
         '--boards=%s' % ':'.join(boards),
         '--overlays=%s' % ':'.join(overlays),
         '--drop_file=%s' % drop_file,
         'commit']
  _RunBuildScript(buildroot, cmd, chromite_cmd=True, enter_chroot=enter_chroot)


def UprevPush(buildroot, overlays, dryrun):
  """Pushes uprev changes to the main line."""
  cmd = ['cros_mark_as_stable',
         '--srcroot=%s' % os.path.join(buildroot, 'src'),
         '--overlays=%s' % ':'.join(overlays)
        ]
  if dryrun:
    cmd.append('--dryrun')
  cmd.append('push')
  _RunBuildScript(buildroot, cmd, chromite_cmd=True)


def AddPackagesForPrebuilt(filename):
  """Add list of packages for upload.

  Process a file that lists all the packages that can be uploaded to the
  package prebuilt bucket and generates the command line args for
  upload_prebuilts.

  Args:
    filename: file with the package full name (category/name-version), one
              package per line.

  Returns:
    A list of parameters for upload_prebuilts. For example:
    ['--packages=net-misc/dhcp', '--packages=app-admin/eselect-python']
  """
  try:
    cmd = []
    with open(filename) as f:
      # Get only the package name and category as that is what upload_prebuilts
      # matches on.
      for line in f:
        atom = line.split('#', 1)[0].strip()
        try:
          cpv = portage_utilities.SplitCPV(atom)
        except ValueError:
          cros_build_lib.Warning('Could not split atom %r (line: %r)',
                                 atom, line)
          continue
        if cpv:
          cmd.extend(['--packages=%s/%s' % (cpv.category, cpv.package)])
    return cmd
  except IOError as e:
    cros_build_lib.Warning('Problem with package file %s' % filename)
    cros_build_lib.Warning('Skipping uploading of prebuilts.')
    cros_build_lib.Warning('ERROR(%d): %s' % (e.errno, e.strerror))
    return None


def _GenerateSdkVersion():
  """Generate a version string for sdk builds

  This needs to be global for test overrides.  It also needs to be done here
  rather than in upload_prebuilts because we want to put toolchain tarballs
  in a specific subdir and that requires keeping the version string in one
  place.  Otherwise we'd have to have the various scripts re-interpret the
  string and try and sync dates across.
  """
  return datetime.now().strftime('%Y.%m.%d.%H%M%S')


def UploadPrebuilts(category, chrome_rev, private_bucket, buildroot, **kwargs):
  """Upload Prebuilts for non-dev-installer use cases.

  Args:
    category: Build type. Can be [binary|full|chrome|chroot|paladin].
    chrome_rev: Chrome_rev of type constants.VALID_CHROME_REVISIONS.
    private_bucket: True if we are uploading to a private bucket.
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine.
    extra_args: Extra args to pass to prebuilts script.
  """
  extra_args = ['--prepend-version', category]
  extra_args.extend(['--upload', 'gs://chromeos-prebuilt'])
  if private_bucket:
    extra_args.extend(['--private', '--binhost-conf-dir',
                       _PRIVATE_BINHOST_CONF_DIR])

  if category == constants.CHROOT_BUILDER_TYPE:
    extra_args.extend(['--sync-host',
                       '--upload-board-tarball'])
    tarball_location = os.path.join(buildroot, 'built-sdk.tar.xz')
    extra_args.extend(['--prepackaged-tarball', tarball_location])

    # See _GenerateSdkVersion comments for more details.
    version = _GenerateSdkVersion()
    extra_args.extend(['--set-version', version])

    # The local tarballs will be simply "<tuple>.tar.xz".  We need
    # them to be "<tuple>-<version>.tar.xz" to avoid collisions.
    for tarball in glob.glob(os.path.join(
        buildroot, constants.DEFAULT_CHROOT_DIR,
        constants.SDK_TOOLCHAINS_OUTPUT, '*.tar.*')):
      tarball_components = os.path.basename(tarball).split('.', 1)

      # Only add the path arg when processing the first tarball.  We do
      # this to get access to the tarball suffix dynamically (so it can
      # change and this code will still work).
      if '--toolchain-upload-path' not in extra_args:
        # Stick the toolchain tarballs into <year>/<month>/ subdirs so
        # we don't start dumping even more stuff into the top level.
        subdir = ('/'.join(version.split('.')[0:2]) + '/' +
                  '%%(target)s-%(version)s.' + tarball_components[1])
        extra_args.extend(['--toolchain-upload-path', subdir])

      arg = '%s:%s' % (tarball_components[0], tarball)
      extra_args.extend(['--toolchain-tarball', arg])

  if category == constants.CHROME_PFQ_TYPE:
    assert chrome_rev
    key = '%s_%s' % (chrome_rev, _CHROME_BINHOST)
    extra_args.extend(['--key', key.upper()])
  elif cbuildbot_config.IsPFQType(category):
    extra_args.extend(['--key', _PREFLIGHT_BINHOST])
  else:
    assert category in (constants.BUILD_FROM_SOURCE_TYPE,
                        constants.CHROOT_BUILDER_TYPE)
    extra_args.extend(['--key', _FULL_BINHOST])

  if category == constants.CHROME_PFQ_TYPE:
    extra_args.extend(['--packages=%s' % constants.CHROME_PN])

  kwargs.setdefault('extra_args', []).extend(extra_args)
  return _UploadPrebuilts(buildroot=buildroot, **kwargs)


class PackageFileMissing(Exception):
  """Raised when the dev installer package file is missing."""
  pass


def UploadDevInstallerPrebuilts(binhost_bucket, binhost_key, binhost_base_url,
                                buildroot, board, **kwargs):
  """Upload Prebuilts for dev-installer use case.

  Args:
    binhost_bucket: bucket for uploading prebuilt packages. If it equals None
                    then the default bucket is used.
    binhost_key: key parameter to pass onto upload_prebuilts. If it equals
                 None, then chrome_rev is used to select a default key.
    binhost_base_url: base url for upload_prebuilts. If None the parameter
                      --binhost-base-url is absent.
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine.
    extra_args: Extra args to pass to prebuilts script.
  """
  extra_args = ['--prepend-version', constants.CANARY_TYPE]
  extra_args.extend(['--binhost-base-url', binhost_base_url])
  extra_args.extend(['--upload', binhost_bucket])
  extra_args.extend(['--key', binhost_key])

  filename = os.path.join(buildroot, 'chroot', 'build', board,
                          _BINHOST_PACKAGE_FILE.lstrip('/'))
  cmd_packages = AddPackagesForPrebuilt(filename)
  if cmd_packages:
    extra_args.extend(cmd_packages)
  else:
    raise PackageFileMissing()

  kwargs.setdefault('extra_args', []).extend(extra_args)
  return _UploadPrebuilts(buildroot=buildroot, board=board, **kwargs)


def _UploadPrebuilts(buildroot, board, extra_args):
  """Upload prebuilts.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine.
    extra_args: Extra args to pass to prebuilts script.
  """
  cwd = constants.CHROMITE_BIN_DIR
  cmd = ['./upload_prebuilts',
         '--build-path', buildroot]

  if board:
    cmd.extend(['--board', board])

  cmd.extend(extra_args)
  _RunBuildScript(buildroot, cmd, cwd=cwd, possibly_flaky=True)


def GenerateCPEExport(buildroot, board, useflags=None):
  """Generate CPE export.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine.
    useflags: A list of useflags for this build.

  Returns:
    A CommandResult object with the results of running the CPE
    export command.
  """
  cmd = ['cros_extract_deps', '--format=cpe', '--board=%s' % board,
         'virtual/target-os']
  env = {}
  if useflags:
    env['USE'] = ' '.join(useflags)
  result = _RunBuildScript(buildroot, cmd, enter_chroot=True,
                           chromite_cmd=True, capture_output=True,
                           extra_env=env)
  return result


def GenerateBreakpadSymbols(buildroot, board, debug):
  """Generate breakpad symbols.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine.
    debug: Include extra debugging output.
  """
  # We don't care about firmware symbols.
  # See http://crbug.com/213670.
  exclude_dirs = ['firmware']

  cmd = ['cros_generate_breakpad_symbols', '--board=%s' % board,
         '--jobs=%s' % str(max([1, multiprocessing.cpu_count() / 2]))]
  cmd += ['--exclude-dir=%s' % x for x in exclude_dirs]
  if debug:
    cmd += ['--debug']
  _RunBuildScript(buildroot, cmd, enter_chroot=True, chromite_cmd=True)


def GenerateDebugTarball(buildroot, board, archive_path, gdb_symbols):
  """Generates a debug tarball in the archive_dir.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine
    archive_path: Directory where tarball should be stored.
    gdb_symbols: Include *.debug files for debugging core files with gdb.

  Returns:
    The filename of the created debug tarball.
  """
  # Generate debug tarball. This needs to run as root because some of the
  # symbols are only readable by root.
  chroot = os.path.join(buildroot, 'chroot')
  board_dir = os.path.join(chroot, 'build', board, 'usr', 'lib')
  debug_tgz = os.path.join(archive_path, 'debug.tgz')
  extra_args = None
  inputs = None

  if gdb_symbols:
    extra_args = ['--exclude',
                  os.path.join('debug', constants.AUTOTEST_BUILD_PATH),
                  '--exclude', 'debug/tests']
    inputs = ['debug']
  else:
    inputs = ['debug/breakpad']

  cros_build_lib.CreateTarball(
      debug_tgz, board_dir, sudo=True, compression=cros_build_lib.COMP_GZIP,
      chroot=chroot, inputs=inputs, extra_args=extra_args)

  # Fix permissions and ownership on debug tarball.
  cros_build_lib.SudoRunCommand(['chown', str(os.getuid()), debug_tgz])
  os.chmod(debug_tgz, 0o644)

  return os.path.basename(debug_tgz)


def GenerateHtmlIndex(index, files, url_base=None, head=None, tail=None):
  """Generate a simple index.html file given a set of filenames

  Args:
    index: The file to write the html index to.
    files: The list of files to create the index of.  If a string, then it
           may be a path to a file (with one file per line), or a directory
           (which will be listed).
    url_base: The URL to prefix to all elements (otherwise they'll be relative).
    head: All the content before the listing.  '<html><body>' if not specified.
    tail: All the content after the listing.  '</body></html>' if not specified.
  """
  def GenLink(target, name=None):
    if name == '':
      return ''
    return ('<li><a href="%s%s">%s</a></li>'
            % (url_base, target, name if name else target))

  if isinstance(files, (unicode, str)):
    if os.path.isdir(files):
      files = os.listdir(files)
    else:
      files = osutils.ReadFile(files).splitlines()
  url_base = url_base + '/' if url_base else ''

  if not head:
    head = '<html><body>'
  html = head + '<ul>'

  dot = ('.',)
  dot_dot = ('..',)
  links = []
  for a in sorted(set(files)):
    a = a.split('|')
    if a[0] == '.':
      dot = a
    elif a[0] == '..':
      dot_dot = a
    else:
      links.append(GenLink(*a))
  links.insert(0, GenLink(*dot_dot))
  links.insert(0, GenLink(*dot))
  html += '\n'.join(links)

  if not tail:
    tail = '</body></html>'
  html += '</ul>' + tail

  osutils.WriteFile(index, html)


def AppendToFile(file_path, string):
  """Append the string to the given file.

  This method provides atomic appends if the string is smaller than
  PIPE_BUF (> 512 bytes). It does not guarantee atomicity once the
  string is greater than that.

  Args:
     file_path: File to be appended to.
     string: String to append to the file.
  """
  osutils.WriteFile(file_path, string, mode='a')


def UpdateUploadedList(last_uploaded, archive_path, upload_url,
                       debug):
  """Updates the list of files uploaded to Google Storage.

  Args:
     buildroot: The root directory where the build occurs.
     last_uploaded: Filename of the last uploaded file.
     archive_path: Path to archive_dir.
     upload_url: Location where tarball should be uploaded.
     debug: Whether we are in debug mode.
  """
  # Append to the uploaded list.
  filename = UPLOADED_LIST_FILENAME
  AppendToFile(os.path.join(archive_path, filename), last_uploaded + '\n')

  # Upload the updated list to Google Storage.
  UploadArchivedFile(archive_path, upload_url, filename, debug,
                     update_list=False)


def UploadArchivedFile(archive_path, upload_url, filename, debug,
                       update_list=False, timeout=2 * 60 * 60, acl=None):
  """Upload the specified file from the archive dir to Google Storage.

  Args:
    archive_path: Path to archive dir.
    upload_url: Location where file should be uploaded.
    debug: Whether we are in debug mode.
    filename: Filename of the file to upload.
    update_list: Flag to update the list of uploaded files.
    timeout: Raise an exception if the upload takes longer than this timeout.
    acl: Canned gsutil acl to use (e.g. 'public-read'), otherwise the internal
         (private) one is used.
  """
  local_path = os.path.join(archive_path, filename)
  gs_context = gs.GSContext(acl=acl, dry_run=debug)

  try:
    with timeout_util.Timeout(timeout):
      gs_context.CopyInto(local_path, upload_url, recursive=True)
  except timeout_util.TimeoutError:
    raise timeout_util.TimeoutError('Timed out uploading %s' % filename)
  else:
    # Update the list of uploaded files.
    if update_list:
      UpdateUploadedList(filename, archive_path, upload_url, debug)


def UploadSymbols(buildroot, board, official, cnt, failed_list):
  """Upload debug symbols for this build."""
  log_cmd = ['upload_symbols', '--board', board, '--dedupe']
  if failed_list is not None:
    log_cmd += ['--failed-list', str(failed_list)]
  if official:
    log_cmd.append('--official_build')
  if cnt is not None:
    log_cmd += ['--upload-limit', str(cnt)]
  cros_build_lib.Info('Running: %s' % cros_build_lib.CmdToStr(log_cmd))

  if official:
    dedupe_namespace = upload_symbols.OFFICIAL_DEDUPE_NAMESPACE
  else:
    dedupe_namespace = upload_symbols.STAGING_DEDUPE_NAMESPACE

  ret = upload_symbols.UploadSymbols(
      board=board, official=official, upload_limit=cnt,
      root=os.path.join(buildroot, constants.DEFAULT_CHROOT_DIR),
      failed_list=failed_list, dedupe_namespace=dedupe_namespace)
  if ret:
    # TODO(davidjames): Convert this to a fatal error.
    # See http://crbug.com/212437
    cros_build_lib.PrintBuildbotStepWarnings()


def PushImages(board, archive_url, dryrun, profile, sign_types=()):
  """Push the generated image to the release bucket for signing."""
  # Log the equivalent command for debugging purposes.
  log_cmd = ['pushimage', '--board=%s' % board]

  if dryrun:
    log_cmd.append('-n')

  if profile:
    log_cmd.append('--profile=%s' % profile)

  if sign_types:
    log_cmd.append('--sign-types=%s' % ' '.join(sign_types))

  log_cmd.append(archive_url)
  cros_build_lib.Info('Running: %s' % cros_build_lib.CmdToStr(log_cmd))

  try:
    return pushimage.PushImage(archive_url, board, profile=profile,
                               sign_types=sign_types, dry_run=dryrun)
  except pushimage.PushError as e:
    cros_build_lib.PrintBuildbotStepFailure()
    return e.args[1]


def BuildFactoryInstallImage(buildroot, board, extra_env):
  """Build a factory install image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine
    extra_env: Flags to be added to the environment for the new process.

  Returns:
    The basename of the symlink created for the image.
  """

  # We use build_attempt=3 here to ensure that this image uses a different
  # output directory from our regular image and the factory test image.
  alias = _FACTORY_SHIM
  cmd = ['./build_image',
         '--board=%s' % board,
         '--replace',
         '--symlink=%s' % alias,
         '--build_attempt=3',
         'factory_install']
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, capture_output=True,
                  enter_chroot=True)
  return alias


def MakeNetboot(buildroot, board, image_dir):
  """Build a netboot image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    image_dir: Directory containing factory install shim.
  """
  cmd = ['./make_netboot.sh',
         '--board=%s' % board,
         '--image_dir=%s' % git.ReinterpretPathForChroot(image_dir)]
  _RunBuildScript(buildroot, cmd, capture_output=True, enter_chroot=True)


def MakeFactoryToolkit(buildroot, board, output_dir, version=None):
  """Build a factory toolkit.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    output_dir: Directory for the resulting factory toolkit.
    version: Version string to be included in ID string.
  """
  cmd = ['./make_factory_toolkit.sh',
         '--board=%s' % board,
         '--output_dir=%s' % git.ReinterpretPathForChroot(output_dir)]
  if version is not None:
    cmd.extend(['--version', version])
  _RunBuildScript(buildroot, cmd, capture_output=True, enter_chroot=True)


def BuildRecoveryImage(buildroot, board, image_dir, extra_env):
  """Build a recovery image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    image_dir: Directory containing base image.
    extra_env: Flags to be added to the environment for the new process.
  """
  image = os.path.join(image_dir, constants.BASE_IMAGE_BIN)
  cmd = ['./mod_image_for_recovery.sh',
         '--board=%s' % board,
         '--image=%s' % git.ReinterpretPathForChroot(image)]
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, capture_output=True,
                  enter_chroot=True)


def BuildTarball(buildroot, input_list, tarball_output, cwd=None,
                 compressed=True, **kwargs):
  """Tars and zips files and directories from input_list to tarball_output.

  Args:
    buildroot: Root directory where build occurs.
    input_list: A list of files and directories to be archived.
    tarball_output: Path of output tar archive file.
    cwd: Current working directory when tar command is executed.
    compressed: Whether or not the tarball should be compressed with pbzip2.
    **kwargs: Keyword arguments to pass to CreateTarball.

  Returns:
    Return value of cros_build_lib.CreateTarball.
  """
  compressor = cros_build_lib.COMP_NONE
  chroot = None
  if compressed:
    compressor = cros_build_lib.COMP_BZIP2
    chroot = os.path.join(buildroot, 'chroot')
  return cros_build_lib.CreateTarball(
      tarball_output, cwd, compression=compressor, chroot=chroot,
      inputs=input_list, **kwargs)


def FindFilesWithPattern(pattern, target='./', cwd=os.curdir):
  """Search the root directory recursively for matching filenames.

  Args:
    pattern: the pattern used to match the filenames.
    target: the target directory to search.
    cwd: current working directory.

  Returns:
    A list of paths of the matched files.
  """
  # Backup the current working directory before changing it
  old_cwd = os.getcwd()
  os.chdir(cwd)

  matches = []
  for target, _, filenames in os.walk(target):
    for filename in fnmatch.filter(filenames, pattern):
      matches.append(os.path.join(target, filename))

  # Restore the working directory
  os.chdir(old_cwd)

  return matches

def BuildAUTestTarball(buildroot, board, work_dir, version, archive_url):
  """Tar up the au test artifacts into the tarball_dir.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    work_dir: Location for doing work.
    version: Basic version of the build i.e. 3289.23.0.
    archive_url: GS directory where we uploaded payloads.
  """
  au_test_tarball = os.path.join(work_dir, 'au_control.tar.bz2')

  cwd = os.path.join(buildroot, 'src', 'third_party', 'autotest', 'files')
  control_files_subdir = os.path.join('autotest', 'au_control_files')

  autotest_dir = os.path.join(work_dir, control_files_subdir)
  os.makedirs(autotest_dir)

  # Get basic version without R*.
  basic_version = re.search('R[0-9]+-([0-9][\w.]+)', version).group(1)

  # Pass in the python paths to the libs full release test needs.
  env_dict = dict(
      chromite_path=buildroot,
      devserver_path=os.path.join(buildroot, 'src', 'platform', 'dev'))

  python_path = '%(chromite_path)s:%(devserver_path)s' % env_dict
  cmd = ['site_utils/autoupdate/full_release_test.py',
         '--npo', '--nmo', '--dump',
         '--dump_dir', autotest_dir, '--archive_url', archive_url,
         basic_version, board, '--log=debug']

  gs_context_dir = os.path.dirname(gs.GSContext.GetDefaultGSUtilBin())
  run_env = None
  if not gs_context_dir in os.environ['PATH']:
    run_env = os.environ.copy()
    run_env['PATH'] += ':%s' % gs_context_dir
  else:
    run_env = os.environ

  run_env.setdefault('PYTHONPATH', '')
  run_env['PYTHONPATH'] += ':%s' % python_path

  cros_build_lib.RunCommand(cmd, env=run_env, cwd=cwd)
  BuildTarball(buildroot, [control_files_subdir], au_test_tarball, cwd=work_dir)
  return au_test_tarball


def BuildFullAutotestTarball(buildroot, board, tarball_dir):
  """Tar up the full autotest directory into image_dir.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    tarball_dir: Location for storing autotest tarballs.

  Returns:
    A tuple the path of the full autotest tarball.
  """

  tarball = os.path.join(tarball_dir, 'autotest.tar.bz2')
  cwd = os.path.abspath(os.path.join(buildroot, 'chroot', 'build', board,
                                     constants.AUTOTEST_BUILD_PATH, '..'))
  result = BuildTarball(buildroot, ['autotest'], tarball, cwd=cwd,
                        error_code_ok=True)

  # Emerging the autotest package to the factory test image while this is
  # running modifies the timestamp on /build/autotest/server by
  # adding a tmp directory underneath it.
  # When tar spots this, it flags this and returns
  # status code 1. The tarball is still OK, although there might be a few
  # unneeded (and garbled) tmp files. If tar fails in a different way, it'll
  # return an error code other than 1.
  # TODO: Fix the autotest ebuild. See http://crbug.com/237537
  if result.returncode not in (0, 1):
    raise Exception('Autotest tarball creation failed with exit code %s'
                    % (result.returncode))

  return tarball


def BuildImageZip(archive_dir, image_dir):
  """Build image.zip in archive_dir from contents of image_dir.

  Exclude the dev image from the zipfile.

  Args:
    archive_dir: Directory to store image.zip.
    image_dir: Directory to zip up.

  Returns:
    The basename of the zipfile.
  """
  filename = 'image.zip'
  zipfile = os.path.join(archive_dir, filename)
  cros_build_lib.RunCommand(['zip', zipfile, '-r', '.'], cwd=image_dir,
                            capture_output=True)
  return filename


def BuildStandaloneArchive(archive_dir, image_dir, artifact_info):
  """Create a compressed archive from the specified image information.

  The artifact info is derived from a JSON file in the board overlay. It
  should be in the following format:
  {
  "artifacts": [
    { artifact },
    { artifact },
    ...
  ]
  }
  Each artifact can contain the following keys:
  input - Required. A list of paths and globs that expands to
      the list of files to archive.
  output - the name of the archive to be created. If omitted,
      it will default to the first filename, stripped of
      extensions, plus the appropriate .tar.gz or other suffix.
  archive - "tar" or "zip". If omitted, files will be uploaded
      directly, without being archived together.
  compress - a value cros_build_lib.CompressionStrToType knows about. Only
      useful for tar. If omitted, an uncompressed tar will be created.

  Args:
    archive_dir: Directory to store image zip.
    image_dir: Base path for all inputs.
    artifact_info: Extended archive configuration dictionary containing:
      - paths - required, list of files to archive.
      - output, archive & compress entries from the JSON file.

  Returns:
    The base name of the archive.

  Raises:
    A ValueError if the compression or archive values are unknown.
    A KeyError is a required field is missing from artifact_info.
  """
  if 'archive' not in artifact_info:
    # Nothing to do, just return the list as-is.
    return artifact_info['paths']

  inputs = artifact_info['paths']
  archive = artifact_info['archive']
  compress = artifact_info.get('compress')
  compress_type = cros_build_lib.CompressionStrToType(compress)
  if compress_type is None:
    raise ValueError('unknown compression type: %s' % compress)

  # If the output is fixed, use that. Otherwise, construct it
  # from the name of the first archived file, stripping extensions.
  filename = artifact_info.get(
      'output', '%s.%s' % (os.path.splitext(inputs[0])[0], archive))
  if archive == 'tar':
    # Add the .compress extension if we don't have a fixed name.
    if 'output' not in artifact_info and compress:
      filename = "%s.%s" % (filename, compress)
    extra_env = { 'XZ_OPT' : '-1' }
    cros_build_lib.CreateTarball(
        os.path.join(archive_dir, filename), image_dir,
        inputs=inputs, compression=compress_type, extra_env=extra_env)
  elif archive == 'zip':
    cros_build_lib.RunCommand(
        ['zip', os.path.join(archive_dir, filename), '-r'] + inputs,
        cwd=image_dir, capture_output=True)
  else:
    raise ValueError('unknown archive type: %s' % archive)

  return [filename]


def BuildFirmwareArchive(buildroot, board, archive_dir):
  """Build firmware_from_source.tar.bz2 in archive_dir from build root.

  Args:
    buildroot: Root directory where build occurs.
    board: Board name of build target.
    archive_dir: Directory to store output file.

  Returns:
    The basename of the archived file, or None if the target board does
    not have firmware from source.
  """
  patterns = ['*image*.bin', 'updater-*.sh', 'ec.bin', 'dts/*']
  firmware_root = os.path.join(buildroot, 'chroot', 'build', board, 'firmware')
  source_list = []
  for pattern in patterns:
    source_list += [os.path.relpath(f, firmware_root)
                    for f in glob.iglob(os.path.join(firmware_root, pattern))]
  if not source_list:
    return None

  archive_name = 'firmware_from_source.tar.bz2'
  archive_file = os.path.join(archive_dir, archive_name)
  BuildTarball(buildroot, source_list, archive_file, cwd=firmware_root)
  return archive_name


def BuildFactoryZip(buildroot, board, archive_dir, factory_shim_dir,
                    factory_toolkit_dir, version=None):
  """Build factory_image.zip in archive_dir.

  Args:
    buildroot: Root directory where build occurs.
    board: Board name of build target.
    archive_dir: Directory to store factory_image.zip.
    factory_shim_dir: Directory containing factory shim.
    factory_toolkit_dir: Directory containing factory toolkit.
    version: The version string to be included in the factory image.zip.

  Returns:
    The basename of the zipfile.
  """
  filename = 'factory_image.zip'

  # Creates a staging temporary folder.
  temp_dir = tempfile.mkdtemp(prefix='cbuildbot_factory')

  zipfile = os.path.join(archive_dir, filename)
  cmd = ['zip', '-r', zipfile, '.']

  # Rules for archive: { folder: pattern }
  rules = {
    factory_shim_dir:
      ['*factory_install*.bin', '*partition*', os.path.join('netboot', '*')],
    factory_toolkit_dir:
      ['*factory_image*.bin', '*partition*', 'install_factory_toolkit.run'],
  }

  for folder, patterns in rules.items():
    if not folder or not os.path.exists(folder):
      continue
    basename = os.path.basename(folder)
    target = os.path.join(temp_dir, basename)
    os.symlink(folder, target)
    for pattern in patterns:
      cmd.extend(['--include', os.path.join(basename, pattern)])

  # Everything in /usr/local/factory/bundle gets overlaid into the
  # bundle.
  bundle_src_dir = os.path.join(
      buildroot, 'chroot', 'build', board, 'usr', 'local', 'factory', 'bundle')
  if os.path.exists(bundle_src_dir):
    for f in os.listdir(bundle_src_dir):
      src_path = os.path.join(bundle_src_dir, f)
      os.symlink(src_path, os.path.join(temp_dir, f))
      cmd.extend(['--include',
                  f if os.path.isfile(src_path) else
                  os.path.join(f, '*')])

  # Add a version file in the zip file.
  if version is not None:
    version_file = os.path.join(temp_dir, 'BUILD_VERSION')
    osutils.WriteFile(version_file, version)
    cmd.extend(['--include', version_file])

  cros_build_lib.RunCommand(cmd, cwd=temp_dir, capture_output=True)
  osutils.RmDir(temp_dir)
  return filename


def ArchiveHWQual(buildroot, hwqual_name, archive_dir, image_dir):
  """Create a hwqual tarball in archive_dir.

  Args:
    buildroot: Root directory where build occurs.
    hwqual_name: Name for tarball.
    archive_dir: Local directory for hwqual tarball.
    image_dir: Directory containing test image.
  """
  scripts_dir = os.path.join(buildroot, 'src', 'scripts')
  cmd = [os.path.join(scripts_dir, 'archive_hwqual'),
         '--from', archive_dir,
         '--image_dir', image_dir,
         '--output_tag', hwqual_name]
  cros_build_lib.RunCommand(cmd, capture_output=True)
  return '%s.tar.bz2' % hwqual_name


def RemoveOldArchives(bot_archive_root, keep_max):
  """Remove old archive directories in bot_archive_root.

  Args:
    bot_archive_root: Parent directory containing old directories.
    keep_max: Maximum number of directories to keep.
  """
  # TODO(davidjames): Reimplement this in Python.
  # +2 because line numbers start at 1 and need to skip LATEST file
  cmd = 'ls -t1 | tail --lines=+%d | xargs rm -rf' % (keep_max + 2)
  cros_build_lib.RunCommand(cmd, cwd=bot_archive_root, shell=True,
                            capture_output=True)


def CreateTestRoot(build_root):
  """Returns a temporary directory for test results in chroot.

  Returns:
    The path inside the chroot rather than whole path.
  """
  # Create test directory within tmp in chroot.
  chroot = os.path.join(build_root, 'chroot')
  chroot_tmp = os.path.join(chroot, 'tmp')
  test_root = tempfile.mkdtemp(prefix='cbuildbot', dir=chroot_tmp)

  # Path inside chroot.
  return os.path.sep + os.path.relpath(test_root, start=chroot)


def GenerateFullPayload(build_root, target_image_path, archive_dir):
  """Generates the full and stateful payloads for hw testing.

  Args:
    build_root: The root of the chromium os checkout.
    target_image_path: The path to the image to generate payloads to.
    archive_dir: Where to store payloads we generated.
  """
  crostestutils = os.path.join(build_root, 'src', 'platform', 'crostestutils')
  payload_generator = 'generate_test_payloads/cros_generate_test_payloads.py'
  cmd = [os.path.join(crostestutils, payload_generator),
         '--target=%s' % target_image_path,
         '--full_payload',
         '--nplus1_archive_dir=%s' % archive_dir,
        ]
  cros_build_lib.RunCommand(cmd, capture_output=True)


def GenerateNPlus1Payloads(build_root, previous_versions_dir, target_image_path,
                           archive_dir):
  """Generates nplus1 payloads for hw testing.

  We generate the nplus1 payloads for testing. These include the full and
  stateful payloads. In addition we generate the n-1->n and n->n delta payloads.

  Args:
    build_root: The root of the chromium os checkout.
    previous_versions_dir: Path containing images for versions previously
      archived.
    target_image_path: The path to the image to generate payloads to.
    archive_dir: Where to store payloads we generated.
  """
  crostestutils = os.path.join(build_root, 'src', 'platform', 'crostestutils')
  payload_generator = 'generate_test_payloads/cros_generate_test_payloads.py'
  cmd = [os.path.join(crostestutils, payload_generator),
         '--target=%s' % target_image_path,
         '--base_latest_from_dir=%s' % previous_versions_dir,
         '--nplus1',
         '--nplus1_archive_dir=%s' % archive_dir,
        ]
  cros_build_lib.RunCommand(cmd)


def GetChromeLKGM(svn_revision):
  """Returns the ChromeOS LKGM from Chrome given the SVN revision."""
  svn_url = '/'.join([gclient.GetBaseURLs()[0], constants.SVN_CHROME_LKGM])
  svn_revision_args = []
  if svn_revision:
    svn_revision_args = ['-r', str(svn_revision)]

  svn_cmd = ['svn', 'cat', svn_url] + svn_revision_args
  return cros_build_lib.RunCommand(svn_cmd, capture_output=True).output.strip()


def SyncChrome(build_root, chrome_root, useflags, tag=None, revision=None):
  """Sync chrome.

  Args:
    build_root: The root of the chromium os checkout.
    chrome_root: The directory where chrome is stored.
    useflags: Array of use flags.
    tag: If supplied, the Chrome tag to sync.
    revision: If supplied, the Chrome revision to sync.
  """
  # --reset tells sync_chrome to blow away local changes and to feel
  # free to delete any directories that get in the way of syncing. This
  # is needed for unattended operation.
  sync_chrome = os.path.join(build_root, 'chromite', 'bin', 'sync_chrome')
  internal = constants.USE_CHROME_INTERNAL in useflags
  cmd = [sync_chrome, '--reset']
  cmd += ['--internal'] if internal else []
  cmd += ['--pdf'] if constants.USE_CHROME_PDF in useflags else []
  cmd += ['--tag', tag] if tag is not None else []
  cmd += ['--revision', revision] if revision is not None else []
  cmd += [chrome_root]
  retry_util.RunCommandWithRetries(constants.SYNC_RETRIES, cmd, cwd=build_root)


def CheckPGOData(architectures, cpv):
  """Check whether PGO data exists for the given architectures.

  Args:
    architectures: Set of architectures we're going to build Chrome for.
    cpv: The portage_utilities.CPV object for chromeos-chrome.

  Returns:
    True if PGO data is available; false otherwise.
  """
  gs_context = gs.GSContext()
  for arch in architectures:
    url = constants.CHROME_PGO_URL % {'package': cpv.package, 'arch': arch,
                                      'version_no_rev': cpv.version_no_rev}
    if not gs_context.Exists(url):
      return False
  return True


class MissingPGOData(results_lib.StepFailure):
  """Exception thrown when necessary PGO data is missing."""


def WaitForPGOData(architectures, cpv, timeout=constants.PGO_USE_TIMEOUT):
  """Wait for PGO data to show up (with an appropriate timeout).

  Args:
    architectures: Set of architectures we're going to build Chrome for.
    cpv: CPV object for Chrome.
    timeout: How long to wait total, in seconds.
  """
  end_time = time.time() + timeout
  while time.time() < end_time:
    if CheckPGOData(architectures, cpv):
      cros_build_lib.Info('Found PGO data')
      return
    cros_build_lib.Info('Waiting for PGO data')
    time.sleep(constants.SLEEP_TIMEOUT)
  raise MissingPGOData('Could not find necessary PGO data.')


def PatchChrome(chrome_root, patch, subdir):
  """Apply a patch to Chrome.

   Args:
     chrome_root: The directory where chrome is stored.
     patch: Rietveld issue number to apply.
     subdir: Subdirectory to apply patch in.
  """
  cmd = ['apply_issue', '-i', patch]
  cros_build_lib.RunCommand(cmd, cwd=os.path.join(chrome_root, subdir))


class ChromeSDK(object):
  """Wrapper for the 'cros chrome-sdk' command."""

  DEFAULT_TARGETS = ('chrome', 'chrome_sandbox', 'nacl_helper',)
  DEFAULT_JOBS = 24
  DEFAULT_JOBS_GOMA = 500

  def __init__(self, cwd, board, extra_args=None, chrome_src=None, goma=False,
               debug_log=True, cache_dir=None):
    """Initialization.

    Args:
      cwd: Where to invoke 'cros chrome-sdk'.
      board: The board to run chrome-sdk for.
      extra_args: Extra args to pass in on the command line.
      chrome_src: Path to pass in with --chrome-src.
      goma: If True, run using goma.
      debug_log: If set, run with debug log-level.
      cache_dir: Specify non-default cache directory.
    """
    self.cwd = cwd
    self.board = board
    self.extra_args = extra_args or []
    if chrome_src:
      self.extra_args += ['--chrome-src', chrome_src]
    self.goma = goma
    if not self.goma:
      self.extra_args.append('--nogoma')
    self.debug_log = debug_log
    self.cache_dir = cache_dir

  def Run(self, cmd, extra_args=None):
    """Run a command inside the chrome-sdk context."""
    cros_cmd = ['cros']
    if self.debug_log:
      cros_cmd += ['--log-level', 'debug']
    if self.cache_dir:
      cros_cmd += ['--cache-dir', self.cache_dir]
    cros_cmd += ['chrome-sdk', '--board', self.board] + self.extra_args
    cros_cmd += (extra_args or []) + ['--'] + cmd
    cros_build_lib.RunCommand(cros_cmd, cwd=self.cwd)

  def Ninja(self, jobs=None, debug=False, targets=DEFAULT_TARGETS):
    """Run 'ninja' inside a chrome-sdk context.

    Args:
      jobs: The number of -j jobs to run.
      debug: Whether to do a Debug build (defaults to Release).
      targets: The targets to compile.
    """
    if jobs is None:
      jobs = self.DEFAULT_JOBS_GOMA if self.goma else self.DEFAULT_JOBS
    flavor = 'Debug' if debug else 'Release'
    cmd = ['ninja', '-C', 'out_%s/%s' % (self.board, flavor) , '-j', str(jobs)]
    self.Run(cmd + list(targets))
