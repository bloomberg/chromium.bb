# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various individual commands a builder can run."""

import fnmatch
import getpass
import glob
import logging
import multiprocessing
import os
import re
import shutil
import tempfile

from chromite.buildbot import cbuildbot_background as background
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.lib import cros_build_lib
from chromite.lib import locking
from chromite.lib import osutils

_DEFAULT_RETRIES = 3
_PACKAGE_FILE = '%(buildroot)s/src/scripts/cbuildbot_package.list'
CHROME_KEYWORDS_FILE = ('/build/%(board)s/etc/portage/package.keywords/chrome')
_PREFLIGHT_BINHOST = 'PREFLIGHT_BINHOST'
_CHROME_BINHOST = 'CHROME_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
_FACTORY_SHIM = 'factory_shim'
_FACTORY_TEST = 'factory_test'
_FACTORY_HWID = 'hwid'
_FULL_BINHOST = 'FULL_BINHOST'
_PRIVATE_BINHOST_CONF_DIR = ('src/private-overlays/chromeos-partner-overlay/'
                             'chromeos/binhost')
_GSUTIL_PATH = '/b/build/scripts/slave/gsutil'
_GS_ACL = '/home/%(user)s/slave_archive_acl' % {'user' : getpass.getuser()}
_BINHOST_PACKAGE_FILE = '/etc/portage/make.profile/package.installable'
_AUTOTEST_RPC_CLIENT = ('/b/build_internal/scripts/slave-internal/autotest_rpc/'
                        'autotest_rpc_client.py')
_LOCAL_BUILD_FLAGS = ['--nousepkg', '--reuse_pkgs_from_local_boards']
_UPLOADED_LIST_FILENAME = 'UPLOADED'

class TestFailure(results_lib.StepFailure):
  pass


# =========================== Command Helpers =================================


def _RunBuildScript(buildroot, cmd, capture_output=False, **kwargs):
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
    capture_output: Whether or not to capture all output.
    kwargs: Optional args passed to RunCommand; see RunCommand for specifics.
  """
  assert not kwargs.get('shell', False), 'Cannot execute shell commands'
  kwargs.setdefault('cwd', buildroot)

  # If we are entering the chroot, create status file for tracking what
  # packages failed to build.
  chroot_tmp = os.path.join(buildroot, 'chroot', 'tmp')
  status_file = None
  enter_chroot = kwargs.get('enter_chroot', False)
  with cros_build_lib.ContextManagerStack() as stack:
    if enter_chroot and os.path.exists(chroot_tmp):
      kwargs['extra_env'] = (kwargs.get('extra_env') or {}).copy()
      status_file = stack.Add(tempfile.NamedTemporaryFile, dir=chroot_tmp)
      kwargs['extra_env']['PARALLEL_EMERGE_STATUS_FILE'] = \
          cros_build_lib.ReinterpretPathForChroot(status_file.name)

    try:
      if capture_output:
        return cros_build_lib.RunCommandCaptureOutput(cmd, **kwargs)
      else:
        return cros_build_lib.RunCommand(cmd, **kwargs)
    except cros_build_lib.RunCommandError as ex:
      # Print the original exception.
      cros_build_lib.Error('\n%s', ex)

      # Check whether a specific package failed. If so, wrap the
      # exception appropriately.
      if status_file is not None:
        status_file.seek(0)
        failed_packages = status_file.read().split()
        if failed_packages:
          raise results_lib.PackageBuildFailure(ex, cmd[0], failed_packages)

      # Looks like a generic failure. Raise a BuildScriptFailure.
      raise results_lib.BuildScriptFailure(ex, cmd[0])


def _GetVMConstants(buildroot):
  """Returns minimum (vdisk_size, statefulfs_size) recommended for VM's."""
  cwd = os.path.join(buildroot, 'src', 'scripts', 'lib')
  source_cmd = 'source %s/cros_vm_constants.sh' % cwd
  vdisk_size = cros_build_lib.RunCommandCaptureOutput(
      ['/bin/bash', '-c', '%s && echo $MIN_VDISK_SIZE_FULL' % source_cmd]
      ).output.strip()
  statefulfs_size = cros_build_lib.RunCommandCaptureOutput(
      ['/bin/bash', '-c', '%s && echo $MIN_STATEFUL_FS_SIZE_FULL' % source_cmd],
       ).output.strip()
  return (vdisk_size, statefulfs_size)


def GetInput(prompt):
  """Helper function to grab input from a user.   Makes testing easier."""
  return raw_input(prompt)


def ValidateClobber(buildroot):
  """Do due diligence if user wants to clobber buildroot.

    buildroot: buildroot that's potentially clobbered.
  Returns: True if the clobber is ok.
  """
  cwd = os.path.dirname(os.path.realpath(__file__))
  if cwd.startswith(buildroot):
    cros_build_lib.Die('You are trying to clobber this chromite checkout!')

  if os.path.exists(buildroot):
    warning = 'This will delete %s' % buildroot
    response = cros_build_lib.YesNoPrompt(default=cros_build_lib.NO,
                                          warning=warning, full=True)
    return response == cros_build_lib.YES


# =========================== Main Commands ===================================

def BuildRootGitCleanup(buildroot, debug_run):
  """Put buildroot onto manifest branch. Delete branches created on last run.

  Args:
    buildroot: buildroot to clean up.
    debug_run: whether the job is running with the --debug flag.  i.e., whether
               it is not a production run.
  """
  lock_path = os.path.join(buildroot, '.clean_lock')

  def RunCleanupCommands(cwd):
    with locking.FileLock(lock_path, verbose=False).read_lock() as lock:
      if not os.path.isdir(cwd):
        return

      try:
        cros_build_lib.GitCleanAndCheckoutUpstream(cwd, False)
      except cros_build_lib.RunCommandError, e:
        result = e.result
        logging.warn('\n%s', result.output)
        logging.warn('Deleting %s because %s failed', cwd, e.result.cmd)
        lock.write_lock()
        if os.path.isdir(cwd):
          shutil.rmtree(cwd)
        # Delete the backing store as well for production jobs, because we
        # want to make sure any corruption is wiped.  Don't do it for
        # tryjobs so the error is visible and can be debugged.
        if not debug_run:
          relpath = os.path.relpath(cwd, buildroot)
          projects_dir = os.path.join(buildroot, '.repo', 'projects')
          repo_store = '%s.git' % os.path.join(projects_dir, relpath)
          logging.warn('Deleting %s as well', repo_store)
          if os.path.isdir(repo_store):
            shutil.rmtree(repo_store)
        cros_build_lib.PrintBuildbotStepWarnings()
        return

      cros_build_lib.RunGitCommand(
          cwd, ['branch', '-D'] + list(constants.CREATED_BRANCHES),
          error_code_ok=True)

  # Cleanup all of the directories.
  dirs = [[os.path.join(buildroot, attrs['path'])] for attrs in
          cros_build_lib.ManifestCheckout.Cached(buildroot).projects.values()]
  background.RunTasksInProcessPool(RunCleanupCommands, dirs)


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
    cros_build_lib.SudoRunCommand(['umount', '-l', mount_pt])


def WipeOldOutput(buildroot):
  """Wipes out build output directory.

  Args:
    buildroot: Root directory where build occurs.
    board: Delete image directories for this board name.
  """
  image_dir = os.path.join('src', 'build', 'images')
  cros_build_lib.SudoRunCommand(['rm', '-rf', image_dir], cwd=buildroot)


def MakeChroot(buildroot, replace, use_sdk, chrome_root=None, extra_env=None):
  """Wrapper around make_chroot."""
  cmd = ['cros_sdk']
  if use_sdk:
    cmd.append('--download')
  else:
    cmd.append('--bootstrap')

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
  cwd = os.path.join(buildroot, 'src', 'scripts')
  chromite_bin_dir = os.path.join('..', '..', constants.CHROMITE_BIN_SUBDIR)

  # First run check_gdata_token to validate or refresh auth token.
  cmd = [os.path.join(chromite_bin_dir, 'check_gdata_token')]
  _RunBuildScript(buildroot, cmd, cwd=cwd)

  # Prepare refresh_package_status command to update the package spreadsheet.
  cmd = [os.path.join(chromite_bin_dir, 'refresh_package_status')]

  # Skip the host board if present.
  board = ':'.join([b for b in boards if b != 'amd64-host'])
  cmd.append('--board=%s' % board)

  # Upload to the test spreadsheet only when in debug mode.
  if debug:
    cmd.append('--test-spreadsheet')

  # Actually run prepared refresh_package_status command.
  _RunBuildScript(buildroot, cmd, enter_chroot=True)

  # Run sync_package_status to create Tracker issues for outdated
  # packages.  At the moment, this runs only for groups that have opted in.
  basecmd = [os.path.join(chromite_bin_dir, 'sync_package_status')]
  if debug:
    basecmd.extend(['--pretend', '--test-spreadsheet'])

  cmdargslist = [['--team=build'],
                 ['--team=kernel', '--default-owner=arscott'],
                 ]

  for cmdargs in cmdargslist:
    cmd = basecmd + cmdargs
    _RunBuildScript(buildroot, cmd, enter_chroot=True)


def SetupBoard(buildroot, board, usepkg, latest_toolchain, extra_env=None,
               profile=None, chroot_upgrade=True):
  """Wrapper around setup_board."""
  cmd = ['./setup_board', '--board=%s' % board]

  # This isn't the greatest thing, but emerge's dependency calculation
  # isn't the speediest thing, so let callers skip this step when they
  # know the system is up-to-date already.
  if not chroot_upgrade:
    cmd.append('--skip_chroot_upgrade')

  if profile:
    cmd.append('--profile=%s' % profile)

  if not usepkg:
    cmd.extend(_LOCAL_BUILD_FLAGS)

  if latest_toolchain:
    cmd.append('--latest_toolchain')

  _RunBuildScript(buildroot, cmd, extra_env=extra_env, enter_chroot=True)


def Build(buildroot, board, build_autotest, usepkg, skip_toolchain_update,
          nowithdebug, extra_env=None, chrome_root=None):
  """Wrapper around build_packages."""
  cmd = ['./build_packages', '--board=%s' % board]
  if not build_autotest: cmd.append('--nowithautotest')

  if skip_toolchain_update: cmd.append('--skip_toolchain_update')

  if not usepkg:
    cmd.extend(_LOCAL_BUILD_FLAGS)

  if nowithdebug:
    cmd.append('--nowithdebug')

  chroot_args = []
  if chrome_root:
    chroot_args.append('--chrome_root=%s' % chrome_root)

  _RunBuildScript(buildroot, cmd, extra_env=extra_env, chroot_args=chroot_args,
                  enter_chroot=True)


def BuildImage(buildroot, board, images_to_build, version='', extra_env=None,
               root_boost=None):
  # Default to base if images_to_build is passed empty.
  if not images_to_build: images_to_build = ['base']
  version_str = '--version=%s' % version

  cmd = ['./build_image', '--board=%s' % board, '--replace', version_str]
  if root_boost is not None:
    cmd += ['--rootfs_boost_size=%d' % root_boost]
  cmd += images_to_build
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, enter_chroot=True)


def BuildVMImageForTesting(buildroot, board, extra_env=None):
  (vdisk_size, statefulfs_size) = _GetVMConstants(buildroot)
  cmd = ['./image_to_vm.sh', '--board=%s' % board, '--test_image',
         '--full', '--vdisk_size=%s' % vdisk_size,
         '--statefulfs_size=%s' % statefulfs_size]
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, enter_chroot=True)


def RunUnitTests(buildroot, board, full, nowithdebug):
  cmd = ['cros_run_unit_tests', '--board=%s' % board]

  if nowithdebug:
    cmd.append('--nowithdebug')

  # If we aren't running ALL tests, then restrict to just the packages
  #   uprev noticed were changed.
  if not full:
    cmd += ['--package_file=%s' %
            cros_build_lib.ReinterpretPathForChroot(_PACKAGE_FILE %
                                                    {'buildroot': buildroot})]

  _RunBuildScript(buildroot, cmd, enter_chroot=True)


def RunChromeSuite(buildroot, board, image_dir, results_dir):
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  if os.path.exists(results_dir_in_chroot):
    shutil.rmtree(results_dir_in_chroot)

  image_path = os.path.join(image_dir, 'chromiumos_qemu_image.bin')

  cwd = os.path.join(buildroot, 'src', 'scripts')
  # TODO(cmasone): make this look for ALL desktopui_BrowserTest control files.
  cmd = ['bin/cros_run_parallel_vm_tests',
         '--board=%s' % board,
         '--quiet',
         '--image_path=%s' % image_path,
         '--results_dir_root=%s' % results_dir,
         'desktopui_BrowserTest.control$',
         'desktopui_BrowserTest.control.one',
         'desktopui_BrowserTest.control.two',
         'desktopui_BrowserTest.control.three',
         'desktopui_PyAutoFunctionalTests.control.vm']
  cros_build_lib.RunCommand(cmd, cwd=cwd, error_ok=True)


def RunTestSuite(buildroot, board, image_dir, results_dir, test_type,
                 whitelist_chrome_crashes, build_config):
  """Runs the test harness suite."""
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  if os.path.exists(results_dir_in_chroot):
    shutil.rmtree(results_dir_in_chroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')

  cmd = ['bin/ctest',
         '--board=%s' % board,
         '--type=vm',
         '--no_graphics',
         '--target_image=%s' % image_path,
         '--test_results_root=%s' % results_dir_in_chroot
        ]

  if test_type != constants.FULL_AU_TEST_TYPE:
    cmd.append('--quick')
    if test_type == constants.SMOKE_SUITE_TEST_TYPE:
      cmd.append('--only_verify')
  else:
    cmd.append('--build_config=%s' % build_config)

  if whitelist_chrome_crashes:
    cmd.append('--whitelist_chrome_crashes')

  result = cros_build_lib.RunCommand(cmd, cwd=cwd, error_ok=True)
  if result.returncode:
    if os.path.exists(results_dir_in_chroot):
      error = '%s exited with code %d' % (' '.join(cmd), result.returncode)
      with open(results_dir_in_chroot + '/failed_test_command', 'w') as failed:
        failed.write(error)
    raise TestFailure('** VMTests failed with code %d **'
                      % result.returncode)


def ArchiveTestResults(buildroot, test_results_dir, prefix):
  """Archives the test results into a tarball.

  Arguments:
    buildroot: Root directory where build occurs.
    test_results_dir: Path from buildroot/chroot to find test results.
      This must a subdir of /tmp.

  Raises:
    No exceptions should be raised. Callers depend on this behaviour.

  Returns the path to the tarball.
  """
  try:
    test_results_dir = test_results_dir.lstrip('/')
    chroot = os.path.join(buildroot, 'chroot')
    results_path = os.path.join(chroot, test_results_dir)
    cros_build_lib.SudoRunCommand(['chmod', '-R', 'a+rw', results_path],
                                  print_cmd=False)

    test_tarball = os.path.join(buildroot, '%stest_results.tgz' % prefix)
    if os.path.exists(test_tarball):
      os.remove(test_tarball)
    gzip = cros_build_lib.FindCompressor(
        cros_build_lib.COMP_GZIP, chroot=chroot)
    cros_build_lib.RunCommand(
        ['tar', '-I', gzip, '-cf', test_tarball,
         '--directory=%s' % results_path, '.'],
        print_cmd=False)
    shutil.rmtree(results_path)

    return test_tarball

  # pylint: disable=W0703
  except Exception, e:
    cros_build_lib.Warning(
        '========================================================')
    cros_build_lib.Warning(
        '------>  We failed to archive test results. <-----------')
    cros_build_lib.Warning(str(e))
    cros_build_lib.Warning(
        '========================================================')


def RunHWTestSuite(build, suite, board, pool, num, debug):
  """Run the test suite in the Autotest lab.

  Args:
    build: The build is described as the bot_id and the build version.
      e.g. x86-mario-release/R18-1655.0.0-a1-b1584.
    suite: Name of the Autotest suite.
    board: The board the test suite should be scheduled against.
    pool: The pool of machines we should use to run the hw tests on.
    num: Number of devices to use when scheduling tests in the hw lab.
    debug: Whether we are in debug mode.
  """
  # TODO(scottz): RPC client option names are misnomers crosbug.com/26445.
  cmd = [_AUTOTEST_RPC_CLIENT,
         'master2', # TODO(frankf): Pass master_host param to cbuildbot.
         'RunSuite',
         '-i', build,
         '-s', suite,
         '-b', board,
         '-p', pool,
         '-u', str(num)]
  if debug:
    cros_build_lib.Info('RunHWTestSuite would run: %s' % ' '.join(cmd))
  else:
    cros_build_lib.RunCommand(cmd)


def GenerateMinidumpStackTraces(buildroot, board, gzipped_test_tarball,
                                archive_dir):
  """Generates stack traces for all minidumps in the gzipped_test_tarball.

  Arguments:
    buildroot: Root directory where build occurs.
    board: Name of the board being worked on.
    gzipped_test_tarball: Path to the gzipped test tarball.
    archive_dir: Local directory for archiving.

  Returns:
    List of stack trace file names.
  """
  stack_trace_filenames = []
  chroot = os.path.join(buildroot, 'chroot')
  gzip = cros_build_lib.FindCompressor(cros_build_lib.COMP_GZIP, chroot=chroot)
  chroot_tmp = os.path.join(chroot, 'tmp')
  temp_dir = tempfile.mkdtemp(prefix='cbuildbot_dumps', dir=chroot_tmp)

  # We need to unzip the test results tarball first because we cannot update
  # a compressed tarball.
  cros_build_lib.RunCommand([gzip, '-df', gzipped_test_tarball])
  test_tarball = os.path.splitext(gzipped_test_tarball)[0] + '.tar'

  # Do our best to generate the symbols but if we fail, don't break the
  # build process.
  tar_cmd = cros_build_lib.RunCommand(
      ['tar', 'xf', test_tarball, '--directory=%s' % temp_dir,
       '--wildcards', '*.dmp'], error_ok=True, redirect_stderr=True)
  if not tar_cmd.returncode:
    symbol_dir = os.path.join('/build', board, 'usr', 'lib', 'debug',
                              'breakpad')
    for curr_dir, _subdirs, files in os.walk(temp_dir):
      for curr_file in files:
        # Skip crash files that were purposely generated.
        if curr_file.find('crasher_nobreakpad') == 0: continue
        full_file_path = os.path.join(curr_dir, curr_file)
        minidump = cros_build_lib.ReinterpretPathForChroot(full_file_path)
        minidump_stack_trace = '%s.txt' % full_file_path
        cwd = os.path.join(buildroot, 'src', 'scripts')
        cros_build_lib.RunCommand(
            ['minidump_stackwalk', minidump, symbol_dir], cwd=cwd,
            enter_chroot=True, error_ok=True, redirect_stderr=True,
            log_stdout_to_file=minidump_stack_trace)
        filename = ArchiveFile(minidump_stack_trace, archive_dir)
        stack_trace_filenames.append(filename)
    cros_build_lib.RunCommand(['tar', 'uf', test_tarball,
                               '--directory=%s' % temp_dir, '.'])
  cros_build_lib.RunCommand('%s -c %s > %s'
                            % (gzip, test_tarball, gzipped_test_tarball),
                            shell=True)
  os.unlink(test_tarball)
  shutil.rmtree(temp_dir)

  return stack_trace_filenames


def ArchiveFile(file_to_archive, archive_dir):
  """Archives the specified file.

  Arguments:
    file_to_archive: Full path to file to archive.
    archive_dir: Local directory for archiving.

  Returns:
    The base name of the archived file.
  """
  filename = os.path.basename(file_to_archive)
  if archive_dir:
    archived_file = os.path.join(archive_dir, filename)
    shutil.copy(file_to_archive, archived_file)
    os.chmod(archived_file, 0644)

  return filename


def MarkChromeAsStable(buildroot,
                       tracking_branch,
                       chrome_rev,
                       boards,
                       chrome_root=None,
                       chrome_version=None):
  """Returns the portage atom for the revved chrome ebuild - see man emerge."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  extra_env = None
  chroot_args = None
  if chrome_root:
    chroot_args = ['--chrome_root=%s' % chrome_root]
    assert chrome_rev == constants.CHROME_REV_LOCAL, (
        'Cannot rev non-local with a chrome_root')

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
    result = cros_build_lib.RunCommandCaptureOutput(
        ['emerge-%s' % board, '-p', '--quiet', '=%s' % chrome_atom],
        enter_chroot=True, error_code_ok=True, combine_stdout_stderr=True)
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


def UprevPackages(buildroot, boards, overlays):
  """Uprevs non-browser chromium os packages that have changed."""
  chroot_overlays = [
      cros_build_lib.ReinterpretPathForChroot(path) for path in overlays ]
  cmd = ['cros_mark_as_stable', '--all', '--boards=%s' % ':'.join(boards),
         '--overlays=%s' % ':'.join(chroot_overlays),
         '--drop_file=%s' % cros_build_lib.ReinterpretPathForChroot(
             _PACKAGE_FILE % {'buildroot': buildroot}),
         'commit']
  _RunBuildScript(buildroot, cmd, enter_chroot=True)


def UprevPush(buildroot, overlays, dryrun):
  """Pushes uprev changes to the main line."""
  cwd = os.path.join(buildroot, constants.CHROMITE_BIN_SUBDIR)
  cmd = ['./cros_mark_as_stable',
         '--srcroot=%s' % os.path.join(buildroot, 'src'),
         '--overlays=%s' % ':'.join(overlays)
        ]
  if dryrun:
    cmd.append('--dryrun')
  cmd.append('push')
  _RunBuildScript(buildroot, cmd, cwd=cwd)


def AddPackagesForPrebuilt(filename):
  """Add list of packages for upload.

  Process a file that lists all the packages that can be uploaded to the
  package prebuilt bucket and generates the command line args for upload_prebuilts.

  Args:
    filename: file with the package full name (category/name-version), one
              package per line.

  Returns:
    A list of parameters for upload_prebuilts. For example:
    ['--packages=net-misc/dhcp', '--packages=app-admin/eselect-python']
  """
  try:
    cmd = []
    package_file = open(filename, 'r')
    # Get only the package name and category. For example, given
    # "app-arch/xz-utils-4.999.9_beta" get "app-arch/xz-utils".
    reg_ex = re.compile('[\w-]+/[\w-]+[a-zA-Z]+[0-9]*')
    for line in package_file:
      match = reg_ex.match(line)
      if match is not None:
        package_name = match.group()
        cmd.extend(['--packages=' + package_name])
    package_file.close()
    return cmd
  except IOError as (errno, strerror):
    cros_build_lib.Warning('Problem with package file %s' % filename)
    cros_build_lib.Warning('Skipping uploading of prebuilts.')
    cros_build_lib.Warning('ERROR(%d): %s' % (errno, strerror))
    return None


def UploadPrebuilts(buildroot, board, private_bucket, category,
                    chrome_rev,
                    binhost_bucket=None,
                    binhost_key=None,
                    binhost_base_url=None,
                    use_binhost_package_file=False,
                    git_sync=False,
                    extra_args=None):
  """Upload prebuilts.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine
    private_bucket: True if we are uploading to a private bucket.
    category: Build type. Can be [binary|full|chrome].
    chrome_rev: Chrome_rev of type constants.VALID_CHROME_REVISIONS.
    binhost_bucket: bucket for uploading prebuilt packages. If it equals None
                    then the default bucket is used.
    binhost_key: key parameter to pass onto upload_prebuilts. If it equals
                 None, then chrome_rev is used to select a default key.
    binhost_base_url: base url for upload_prebuilts. If None the parameter
                      --binhost-base-url is absent.
    git_sync: boolean that enables --git-sync upload_prebuilts parameter.
    use_binhost_package_file: use the File that contains the packages to upload
                              to the binhost. If it equals False then all
                              packages are selected.
    extra_args: Extra args to send to upload_prebuilts.
  """
  if extra_args is None:
    extra_args = []

  cwd = constants.CHROMITE_BIN_DIR
  cmd = ['./upload_prebuilts',
         '--build-path', buildroot,
         '--prepend-version', category]

  if binhost_base_url is not None:
    cmd.extend(['--binhost-base-url', binhost_base_url])

  if binhost_bucket is not None:
    cmd.extend(['--upload', binhost_bucket])
  else:
    cmd.extend(['--upload', 'gs://chromeos-prebuilt'])

  if private_bucket:
    cmd.extend(['--private', '--binhost-conf-dir', _PRIVATE_BINHOST_CONF_DIR])

  if board:
    cmd.extend(['--board', board])

  if category == 'chroot':
    cmd.extend(['--sync-host',
                '--upload-board-tarball'])

  if binhost_key is not None:
    cmd.extend(['--key', binhost_key])
  elif category == constants.CHROME_PFQ_TYPE:
    assert chrome_rev
    key = '%s_%s' % (chrome_rev, _CHROME_BINHOST)
    cmd.extend(['--key', key.upper()])
  elif cbuildbot_config.IsPFQType(category):
    cmd.extend(['--key', _PREFLIGHT_BINHOST])
  else:
    assert category in (constants.BUILD_FROM_SOURCE_TYPE,
                        constants.CHROOT_BUILDER_TYPE)
    cmd.extend(['--key', _FULL_BINHOST])

  if category == constants.CHROME_PFQ_TYPE:
    cmd.extend(['--packages=chromeos-chrome'])

  if use_binhost_package_file:
    filename = os.path.join(buildroot, 'chroot', 'build', board,
                            _BINHOST_PACKAGE_FILE.lstrip('/'))
    cmd_packages = AddPackagesForPrebuilt(filename)
    if cmd_packages:
      cmd.extend(cmd_packages)
    else:
      # If there is any problem with the packages file do not upload anything.
      return

  if git_sync:
    cmd.extend(['--git-sync'])

  cmd.extend(extra_args)
  _RunBuildScript(buildroot, cmd, cwd=cwd)


def GenerateBreakpadSymbols(buildroot, board):
  """Generate breakpad symbols.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine
  """
  cmd = ['./cros_generate_breakpad_symbols', '--board=%s' % board]
  extra_env = {'NUM_JOBS': str(max([1, multiprocessing.cpu_count() / 2]))}
  _RunBuildScript(buildroot, cmd, enter_chroot=True, capture_output=True,
                  extra_env=extra_env)


def GenerateDebugTarball(buildroot, board, archive_path, gdb_symbols):
  """Generates a debug tarball in the archive_dir.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine
    archive_dir: Directory where tarball should be stored.
    gdb_symbols: Include *.debug files for debugging core files with gdb.

  Returns the filename of the created debug tarball.
  """

  # Generate debug tarball. This needs to run as root because some of the
  # symbols are only readable by root.
  chroot = os.path.join(buildroot, 'chroot')
  board_dir = os.path.join(chroot, 'build', board, 'usr', 'lib')
  gzip = cros_build_lib.FindCompressor(cros_build_lib.COMP_GZIP, chroot=chroot)
  debug_tgz = os.path.join(archive_path, 'debug.tgz')
  cmd = ['tar', '-I', gzip, '-cf', debug_tgz]
  if gdb_symbols:
    cmd.extend(['--exclude', 'debug/usr/local/autotest',
                '--exclude', 'debug/tests',
                'debug'])
  else:
    cmd.append('debug/breakpad')

  tar_cmd = cros_build_lib.SudoRunCommand(cmd, cwd=board_dir, error_ok=True)

  # Emerging the factory kernel while this is running installs different debug
  # symbols. When tar spots this, it flags this and returns status code 1.
  # The tarball is still OK, although the kernel debug symbols might be garbled.
  # If tar fails in a different way, it'll return an error code other than 1.
  # TODO(davidjames): Remove factory kernel emerge from archive_build.
  if tar_cmd.returncode not in (0, 1):
    raise Exception('%r failed with exit code %s' % (cmd, tar_cmd.returncode))

  # Fix permissions and ownership on debug tarball.
  cros_build_lib.SudoRunCommand(['chown', str(os.getuid()), debug_tgz])
  os.chmod(debug_tgz, 0644)

  return os.path.basename(debug_tgz)


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


def UpdateUploadedList(last_uploaded, archive_path, upload_url, debug):
  """Updates the list of files uploaded to Google Storage.

  Args:
     last_uploaded: Filename of the last uploaded file.
     archive_path: Path to archive_dir.
     upload_url: Location where tarball should be uploaded.
     debug: Whether we are in debug mode.
  """

  # Append to the uploaded list.
  filename = _UPLOADED_LIST_FILENAME
  AppendToFile(os.path.join(archive_path, filename), last_uploaded+'\n')

  # Upload the updated list to Google Storage.
  UploadArchivedFile(archive_path, upload_url, filename, debug,
                     update_list=False)


def UploadArchivedFile(archive_path, upload_url, filename, debug,
                       update_list=False):
  """Upload the specified tarball from the archive dir to Google Storage.

  Args:
    archive_path: Path to archive dir.
    upload_url: Location where tarball should be uploaded.
    debug: Whether we are in debug mode.
    filename: Filename of the tarball to upload.
    update_list: Flag to update the list of uploaded files.
  """

  if upload_url:
    full_filename = os.path.join(archive_path, filename)
    full_url = '%s/%s' % (upload_url, filename)
    cmds = (
        [_GSUTIL_PATH, 'cp', full_filename, full_url],
        [_GSUTIL_PATH, 'setacl', _GS_ACL, full_url]
    )

    for cmd in cmds:
      if debug:
        cros_build_lib.Info('UploadArchivedFile would run: %s' % ' '.join(cmd))
      else:
        cros_build_lib.RunCommandCaptureOutput(cmd)

    # Update the list of uploaded files.
    if update_list:
      UpdateUploadedList(filename, archive_path, upload_url, debug)


def UploadSymbols(buildroot, board, official):
  """Upload debug symbols for this build."""
  cmd = ['./upload_symbols',
        '--board=%s' % board,
        '--yes',
        '--verbose']

  if official:
    cmd += ['--official_build']

  cwd = os.path.join(buildroot, 'src', 'scripts')

  try:
    cros_build_lib.RunCommandCaptureOutput(cmd, cwd=cwd, enter_chroot=True,
                                           combine_stdout_stderr=True)
  except cros_build_lib.RunCommandError, e:
    # TODO(davidjames): Convert this to a fatal error.
    cros_build_lib.PrintBuildbotStepWarnings()
    logging.warn('\n%s', e.result.output)


def PushImages(buildroot, board, branch_name, archive_url, dryrun, profile):
  """Push the generated image to http://chromeos_images."""
  cmd = ['./pushimage', '--board=%s' % board]

  if dryrun:
    cmd.append('-n')

  if profile:
    cmd.append('--profile=%s' % profile)

  if branch_name:
    cmd.append('--branch=%s' % branch_name)

  cmd.append(archive_url)

  cros_build_lib.RunCommand(cmd, cwd=os.path.join(buildroot, 'crostools'))


def BuildFactoryTestImage(buildroot, board, extra_env):
  """Build a factory test image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine
    extra_env: Flags to be added to the environment for the new process.

  Returns the basename of the symlink created for the image.
  """

  # We use build_attempt=2 here to ensure that this image uses a different
  # output directory from our regular image and the factory shim image (below).
  alias = _FACTORY_TEST
  cmd = ['./build_image',
         '--board=%s' % board,
         '--replace',
         '--noenable_rootfs_verification',
         '--symlink=%s' % alias,
         '--build_attempt=2',
         'factory_test']
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, capture_output=True,
                  enter_chroot=True)
  return alias


def BuildFactoryInstallImage(buildroot, board, extra_env):
  """Build a factory install image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine
    extra_env: Flags to be added to the environment for the new process.

  Returns the basename of the symlink created for the image.
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
  """Convert the specified image to be a netboot image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    image_dir: Directory containing factory install shim.
  """
  image = os.path.join(image_dir, 'factory_install_shim.bin')
  cmd = ['./make_netboot.sh',
         '--board=%s' % board,
         '--image=%s' % cros_build_lib.ReinterpretPathForChroot(image)]
  _RunBuildScript(buildroot, cmd, capture_output=True, enter_chroot=True)


def BuildRecoveryImage(buildroot, board, image_dir, extra_env):
  """Build a recovery image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    image_dir: Directory containing base image.
    extra_env: Flags to be added to the environment for the new process.
  """
  image = os.path.join(image_dir, 'chromiumos_base_image.bin')
  cmd = ['./mod_image_for_recovery.sh',
         '--board=%s' % board,
         '--image=%s' % cros_build_lib.ReinterpretPathForChroot(image)]
  _RunBuildScript(buildroot, cmd, extra_env=extra_env, capture_output=True,
                  enter_chroot=True)


def BuildTarball(buildroot, input_list, tarball_output, cwd=None,
                 compressed=True):
  """Tars and zips files and directories from input_list to tarball_output.

  Args:
    buildroot: Root directory where build occurs.
    input_list: A list of files and directories to be archived.
    tarball_output: Path of output tar archive file.
    cwd: Current working directory when tar command is executed.
    compressed: Whether or not the tarball should be compressed with pbzip2.
  """
  cmd = ['tar']
  if compressed:
    bzip2 = cros_build_lib.FindCompressor(
        cros_build_lib.COMP_BZIP2, chroot=os.path.join(buildroot, 'chroot'))
    cmd.append('--use-compress-program=%s' % bzip2)
  cmd += ['-cf', tarball_output] + input_list
  cros_build_lib.RunCommandCaptureOutput(cmd, cwd=cwd)


def FindFilesWithPattern(pattern, target='./', cwd=os.curdir):
  """Search the root directory recursively for matching filenames.

  Args:
    pattern: the pattern used to match the filenames.
    target: the target directory to search.
    cwd: current working directory.

  Returns a list of paths of the matched files.
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

def BuildAutotestTarballs(buildroot, board, tarball_dir):
  """Tar up the autotest artifacts into image_dir.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    tarball_dir: Location for storing autotest tarballs.

  Returns a tuple containing the paths of the autotest and test_suites tarballs.
  """
  autotest_tarball = os.path.join(tarball_dir, 'autotest.tar')
  test_suites_tarball = os.path.join(tarball_dir, 'test_suites.tar.bz2')
  cwd = os.path.join(buildroot, 'chroot', 'build', board, 'usr', 'local')

  # Find the control files in autotest/
  control_files = FindFilesWithPattern('control*', target='autotest', cwd=cwd)

  # Tar the control files and the packages
  input_list = control_files + ['autotest/packages']
  BuildTarball(buildroot, input_list, autotest_tarball,
               cwd=cwd, compressed=False)
  BuildTarball(buildroot, ['autotest/test_suites'], test_suites_tarball,
               cwd=cwd)

  return [autotest_tarball, test_suites_tarball]


def BuildFullAutotestTarball(buildroot, board, tarball_dir):
  """Tar up the full autotest directory into image_dir.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    tarball_dir: Location for storing autotest tarballs.

  Returns a tuple the path of the full autotest tarball.
  """

  tarball = os.path.join(tarball_dir, 'autotest.tar.bz2')
  cwd = os.path.join(buildroot, 'chroot', 'build', board, 'usr', 'local')
  BuildTarball(buildroot, ['autotest'], tarball, cwd=cwd)

  return tarball


def BuildImageZip(archive_dir, image_dir):
  """Build image.zip in archive_dir from contents of image_dir.

  Exclude the dev image from the zipfile.

  Args:
    archive_dir: Directory to store image.zip.
    image_dir: Directory to zip up.

  Returns the basename of the zipfile.
  """
  filename = 'image.zip'
  zipfile = os.path.join(archive_dir, filename)
  cros_build_lib.RunCommandCaptureOutput(['zip', zipfile, '-r', '.'],
                                         cwd=image_dir)
  return filename


def BuildRecoveryImageZip(archive_dir, recovery_image_bin):
  """Build recovery_image.zip in archive_dir from recovery_image.bin

  Args:
    archive_dir: Directory to store recovery_image.zip
    recovery_image_bin: Recovery image to zip up.

    Returns the base name of the zipfile.
  """
  filename = 'recovery_image.zip'
  zipfile = os.path.join(archive_dir, filename)
  cros_build_lib.RunCommandCaptureOutput(
      ['zip', zipfile, os.path.basename(recovery_image_bin)],
      cwd=os.path.dirname(recovery_image_bin))
  return filename


def BuildFirmwareArchive(buildroot, board, archive_dir):
  """Build firmware_from_source.tar.bz2 in archive_dir from build root.

  Args:
    buildroot: Root directory where build occurs.
    board: Board name of build target.
    archive_dir: Directory to store output file.

  Returns the basename of the archived file, or None if the target board does
  not have firmware from source.
  """
  patterns = ['*image*.bin', 'ec.bin']
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


def BuildFactoryZip(buildroot, archive_dir, image_root):
  """Build factory_image.zip in archive_dir.

  Args:
    buildroot: Root directory where build occurs.
    archive_dir: Directory to store image.zip.
    image_root: Directory containing factory_shim and factory_test symlinks.

  Returns the basename of the zipfile.
  """
  filename = 'factory_image.zip'

  # Creates a staging temporary folder.
  temp_dir = tempfile.mkdtemp(prefix='cbuildbot_factory')

  zipfile = os.path.join(archive_dir, filename)
  cmd = ['zip', '-r', zipfile, '.']

  # Rules for archive: { folder: pattern }
  rules = {
    os.path.join(image_root, _FACTORY_SHIM):
      ['*factory_install*.bin', '*partition*', os.path.join('netboot', '*')],
    os.path.join(image_root, _FACTORY_TEST):
      ['*factory_image*.bin', '*partition*'],
    os.path.join(image_root, _FACTORY_TEST, _FACTORY_HWID):
      ['*'],
    os.path.join(buildroot, 'chroot', 'usr', 'share',
                 'cros-factoryutils', 'factory_setup'):
      ['*'],
  }

  # Special files that must not be included.
  excludes_list = [
    os.path.join(_FACTORY_TEST, _FACTORY_HWID, '*'),
    os.path.join('factory_setup', 'static', '*')
  ]

  for folder, patterns in rules.items():
    if not os.path.exists(folder):
      continue
    basename = os.path.basename(folder)
    target = os.path.join(temp_dir, basename)
    os.symlink(folder, target)
    for pattern in patterns:
      cmd.extend(['--include', os.path.join(basename, pattern)])

  for exclude in excludes_list:
    cmd.extend(['--exclude', exclude])

  cros_build_lib.RunCommandCaptureOutput(cmd, cwd=temp_dir)
  shutil.rmtree(temp_dir)
  return filename


def ArchiveHWQual(buildroot, hwqual_name, archive_dir):
  """Create a hwqual tarball in archive_dir.

  Args:
    buildroot: Root directory where build occurs.
    hwqual_name: Name for tarball.
    archive_dir: Local directory for hwqual tarball.
  """
  scripts_dir = os.path.join(buildroot, 'src', 'scripts')
  cmd = [os.path.join(scripts_dir, 'archive_hwqual'),
         '--from', archive_dir,
         '--output_tag', hwqual_name]
  cros_build_lib.RunCommandCaptureOutput(cmd)
  return '%s.tar.bz2' % hwqual_name


def UpdateLatestFile(bot_archive_root, set_version):
  """Update the latest file in archive_root.

  Args:
    bot_archive_root: Parent directory of archive directory.
    set_version: Version of output directory.
  """
  latest_path = os.path.join(bot_archive_root, 'LATEST')
  osutils.WriteFile(latest_path, set_version, mode='w')


def RemoveOldArchives(bot_archive_root, keep_max):
  """Remove old archive directories in bot_archive_root.

  Args:
    bot_archive_root: Parent directory containing old directories.
    keep_max: Maximum number of directories to keep.
  """
  # TODO(davidjames): Reimplement this in Python.
  # +2 because line numbers start at 1 and need to skip LATEST file
  cmd = 'ls -t1 | tail --lines=+%d | xargs rm -rf' % (keep_max + 2)
  cros_build_lib.RunCommandCaptureOutput(cmd, cwd=bot_archive_root, shell=True)


def CreateTestRoot(build_root):
  """Returns a temporary directory for test results in chroot.

  Returns:
    Returns the path inside the chroot rather than whole path.
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
  cros_build_lib.RunCommandCaptureOutput(cmd)


def GenerateNPlus1Payloads(build_root, build_config, target_image_path,
                           archive_dir):
  """Generates nplus1 payloads for hw testing.

  We generate the nplus1 payloads for testing. These include the full and
  stateful payloads. In addition we generate the n-1->n and n->n delta payloads.

  Args:
    build_root: The root of the chromium os checkout.
    build_config: The name of the builder.
    target_image_path: The path to the image to generate payloads to.
    archive_dir: Where to store payloads we generated.
  """
  crostestutils = os.path.join(build_root, 'src', 'platform', 'crostestutils')
  payload_generator = 'generate_test_payloads/cros_generate_test_payloads.py'
  cmd = [os.path.join(crostestutils, payload_generator),
         '--target=%s' % target_image_path,
         '--base_latest_from_config=%s' % build_config,
         '--nplus1',
         '--nplus1_archive_dir=%s' % archive_dir,
        ]
  cros_build_lib.RunCommandCaptureOutput(cmd)
