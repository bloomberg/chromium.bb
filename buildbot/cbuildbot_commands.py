# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various individual commands a builder can run."""

import constants
import getpass
import logging
import os
import re
import shutil
import tempfile

from chromite.buildbot import cbuildbot_background as background
from chromite.buildbot import cbuildbot_config
from chromite.lib import cros_build_lib as cros_lib
from chromite.lib import locking

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

class TestException(Exception):
  pass

# =========================== Command Helpers =================================

def _GetVMConstants(buildroot):
  """Returns minimum (vdisk_size, statefulfs_size) recommended for VM's."""
  cwd = os.path.join(buildroot, 'src', 'scripts', 'lib')
  source_cmd = 'source %s/cros_vm_constants.sh' % cwd
  vdisk_size = cros_lib.RunCommandCaptureOutput([
      '/bin/bash', '-c', '%s && echo $MIN_VDISK_SIZE_FULL' % source_cmd]
      ).output.strip()
  statefulfs_size = cros_lib.RunCommandCaptureOutput([
      '/bin/bash', '-c', '%s && echo $MIN_STATEFUL_FS_SIZE_FULL' % source_cmd],
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
    cros_lib.Die('You are trying to clobber this chromite checkout!')

  if os.path.exists(buildroot):
    warning = 'This will delete %s' % buildroot
    response = cros_lib.YesNoPrompt(default=cros_lib.NO, warning=warning,
                                    full=True)
    return response == cros_lib.YES


# =========================== Main Commands ===================================

def BuildRootGitCleanup(buildroot):
  """Put buildroot onto manifest branch. Delete branches created on last run."""
  manifest_branch = 'remotes/m/' + cros_lib.GetManifestDefaultBranch(buildroot)
  tasks = [
    (False, [['git', 'am', '--abort'],
             ['git', 'rebase', '--abort']]),
    (True, [['git', 'reset', '--hard', 'HEAD'],
             ['git', 'checkout', manifest_branch],
             ['git', 'clean', '-f', '-d']]),
    (False, [['git', 'branch', '-D', b] for b in constants.CREATED_BRANCHES])
  ]
  lock_path = os.path.join(buildroot, '.clean_lock')

  def RunCleanupCommands(cwd):
    with locking.FileLock(lock_path, verbose=False).read_lock() as lock:
      if not os.path.isdir(cwd): return
      for check_returncode, cmds in tasks:
        for cmd in cmds:
          result = cros_lib.RunCommandCaptureOutput(
              cmd, cwd=cwd, error_code_ok=True, print_cmd=False)
          if check_returncode and result.returncode != 0:
            logging.info(result.output)
            logging.warn('Deleting %s because %s failed', cwd, cmd)
            lock.write_lock()
            if os.path.isdir(cwd):
              shutil.rmtree(cwd)
            print '@@@STEP_WARNINGS@@@'
            return

  # Cleanup all of the directories.
  dirs = [[os.path.join(buildroot, attrs['path'])] for attrs in
          cros_lib.ParseFullManifest(buildroot).projects.values()]
  background.RunTasksInProcessPool(RunCleanupCommands, dirs)


def CleanUpMountPoints(buildroot):
  """Cleans up any stale mount points from previous runs."""
  mount_result = cros_lib.RunCommandCaptureOutput(['mount'], print_cmd=False)

  mount_lines = [x for x in mount_result.output.splitlines() if buildroot in x]

  mount_lines = [x.rpartition(' type ')[0].partition(' on ')[2]
                 for x in mount_lines]

  mount_lines.sort(reverse=True)

  for mount_pt in mount_lines:
    cros_lib.SudoRunCommand(['umount', '-l', mount_pt])


def WipeOldOutput(buildroot):
  """Wipes out build output directory.

  Args:
    buildroot: Root directory where build occurs.
    board: Delete image directories for this board name.
  """
  image_dir = os.path.join('src', 'build', 'images')
  cros_lib.RunCommand(['rm', '-rf', image_dir], cwd=buildroot)


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

  cros_lib.RunCommand(cmd, cwd=buildroot, extra_env=extra_env)


def RunChrootUpgradeHooks(buildroot, chrome_root=None):
  """Run the chroot upgrade hooks in the chroot."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  chroot_args = []
  if chrome_root:
    chroot_args.append('--chrome_root=%s' % chrome_root)

  cros_lib.RunCommand(['./run_chroot_version_hooks'], cwd=cwd,
                      enter_chroot=True, chroot_args=chroot_args)


def RefreshPackageStatus(buildroot, boards, debug):
  """Wrapper around refresh_package_status"""
  cwd = os.path.join(buildroot, 'src', 'scripts')

  # First run check_gdata_token to validate or refresh auth token.
  cmd = ['../../chromite/bin/check_gdata_token']
  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=False)

  # Prepare refresh_package_status command.
  cmd = ['../../chromite/bin/refresh_package_status']

  # Skip the host board if present.
  board = ':'.join([b for b in boards if b != 'amd64-host'])
  cmd.append('--board=%s' % board)

  # Upload to the test spreadsheet only when in debug mode.
  if debug:
    cmd.append('--test-spreadsheet')

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True)


def SetupBoard(buildroot, board, usepkg, latest_toolchain, extra_env=None,
               profile=None, chroot_upgrade=True):
  """Wrapper around setup_board."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
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

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, extra_env=extra_env)


def Build(buildroot, board, build_autotest, usepkg, skip_toolchain_update,
          nowithdebug, extra_env=None, chrome_root=None):
  """Wrapper around build_packages."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./build_packages', '--board=%s' % board]
  if extra_env is None:
    env = {}
  else:
    env = extra_env.copy()

  if not build_autotest: cmd.append('--nowithautotest')

  if skip_toolchain_update: cmd.append('--skip_toolchain_update')

  if not usepkg:
    cmd.extend(_LOCAL_BUILD_FLAGS)

  if nowithdebug:
    cmd.append('--nowithdebug')

  chroot_args = []
  if chrome_root:
    chroot_args.append('--chrome_root=%s' % chrome_root)

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, extra_env=env,
                      chroot_args=chroot_args)


def BuildImage(buildroot, board, images_to_build, version='', extra_env=None):
  cwd = os.path.join(buildroot, 'src', 'scripts')
  # Default to base if images_to_build is passed empty.
  if not images_to_build: images_to_build = ['base']
  version_str = '--version=%s' % version

  cmd = ['./build_image', '--board=%s' % board,
         '--replace', version_str] + images_to_build
  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, extra_env=extra_env)


def BuildVMImageForTesting(buildroot, board, extra_env=None):
  (vdisk_size, statefulfs_size) = _GetVMConstants(buildroot)
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cros_lib.RunCommand(['./image_to_vm.sh',
                       '--board=%s' % board,
                       '--test_image',
                       '--full',
                       '--vdisk_size=%s' % vdisk_size,
                       '--statefulfs_size=%s' % statefulfs_size,
                      ], cwd=cwd, enter_chroot=True, extra_env=extra_env)


def RunUnitTests(buildroot, board, full, nowithdebug):
  cwd = os.path.join(buildroot, 'src', 'scripts')

  cmd = ['cros_run_unit_tests', '--board=%s' % board]

  if nowithdebug:
    cmd.append('--nowithdebug')

  # If we aren't running ALL tests, then restrict to just the packages
  #   uprev noticed were changed.
  if not full:
    cmd += ['--package_file=%s' %
            cros_lib.ReinterpretPathForChroot(_PACKAGE_FILE %
                                              {'buildroot': buildroot})]

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True)


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
  cros_lib.RunCommand(cmd, cwd=cwd, error_ok=True, enter_chroot=False)


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

  result = cros_lib.RunCommand(cmd, cwd=cwd, error_ok=True)
  if result.returncode:
    failed = open(results_dir_in_chroot + '/failed_test_command', 'w')
    failed.write('%s exited with code %d' % (' '.join(cmd), result.returncode))
    failed.close()
    raise TestException('** VMTests failed with code %d **'
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
    results_path = os.path.join(buildroot, 'chroot', test_results_dir)
    cros_lib.SudoRunCommand(['chmod', '-R', 'a+rw', results_path],
                            print_cmd=False)

    test_tarball = os.path.join(buildroot, '%stest_results.tgz' % prefix)
    if os.path.exists(test_tarball): os.remove(test_tarball)
    cros_lib.RunCommand(['tar', 'czf', test_tarball,
                         '--directory=%s' % results_path, '.'],
                        print_cmd=False)
    shutil.rmtree(results_path)

    return test_tarball

  except Exception, e:
    cros_lib.Warning('========================================================')
    cros_lib.Warning('------>  We failed to archive test results. <-----------')
    cros_lib.Warning(str(e))
    cros_lib.Warning('========================================================')


def RunHWTestSuite(build, suite, board, debug):
  """Run the test suite in the Autotest lab.

  Args:
    build: The build is described as the bot_id and the build version.
      e.g. x86-mario-release/R18-1655.0.0-a1-b1584.
    suite: Name of the Autotest suite.
    board: The board the test suite should be scheduled against.
    debug: Whether we are in debug mode.
  """
  # TODO(scottz): RPC client option names are misnomers crosbug.com/26445.
  cmd = [_AUTOTEST_RPC_CLIENT,
         'master2', # TODO(frankf): Pass master_host param to cbuildbot.
         'RunSuite',
         '-i', build,
         '-s', suite,
         '-b', board,
         '-p', constants.HWTEST_MACH_POOL]
  if debug:
    cros_lib.Info('RunHWTestSuite would run: %s' % ' '.join(cmd))
  else:
    cros_lib.RunCommand(cmd)


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
  chroot_tmp = os.path.join(buildroot, 'chroot', 'tmp')
  temp_dir = tempfile.mkdtemp(prefix='cbuildbot_dumps', dir=chroot_tmp)

  # We need to unzip the test results tarball first because we cannot update
  # a compressed tarball.
  cros_lib.RunCommand(['gzip', '-df', gzipped_test_tarball])
  test_tarball = os.path.splitext(gzipped_test_tarball)[0] + '.tar'

  # Do our best to generate the symbols but if we fail, don't break the
  # build process.
  tar_cmd = cros_lib.RunCommand(['tar',
                                 'xf',
                                 test_tarball,
                                 '--directory=%s' % temp_dir,
                                 '--wildcards', '*.dmp'],
                                error_ok=True,
                                redirect_stderr=True)
  if not tar_cmd.returncode:
    symbol_dir = os.path.join('/build', board, 'usr', 'lib', 'debug',
                              'breakpad')
    for curr_dir, _subdirs, files in os.walk(temp_dir):
      for curr_file in files:
        # Skip crash files that were purposely generated.
        if curr_file.find('crasher_nobreakpad') == 0: continue
        full_file_path = os.path.join(curr_dir, curr_file)
        minidump = cros_lib.ReinterpretPathForChroot(full_file_path)
        minidump_stack_trace = '%s.txt' % full_file_path
        cwd = os.path.join(buildroot, 'src', 'scripts')
        cros_lib.RunCommand(['minidump_stackwalk',
                             minidump,
                             symbol_dir],
                            cwd=cwd,
                            enter_chroot=True,
                            error_ok=True,
                            redirect_stderr=True,
                            log_stdout_to_file=minidump_stack_trace)
        filename = ArchiveFile(minidump_stack_trace, archive_dir)
        stack_trace_filenames.append(filename)
    cros_lib.RunCommand(['tar',
                         'uf',
                         test_tarball,
                         '--directory=%s' % temp_dir,
                         '.'])
  cros_lib.RunCommand('gzip -c %s > %s' % (test_tarball, gzipped_test_tarball),
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

  command = ['../../chromite/buildbot/cros_mark_chrome_as_stable',
             '--tracking_branch=%s' % tracking_branch,
             '--boards=%s' % ':'.join(boards), ]
  if chrome_version:
    command.append('--force_revision=%s' % chrome_version)
    assert chrome_rev == constants.CHROME_REV_SPEC, (
        'Cannot rev non-spec with a chrome_version')

  portage_atom_string = cros_lib.RunCommand(
      command + [chrome_rev],
      cwd=cwd,
      redirect_stdout=True,
      enter_chroot=True,
      chroot_args=chroot_args,
      extra_env=extra_env).output.rstrip()
  if not portage_atom_string:
    cros_lib.Info('Found nothing to rev.')
    return None
  else:
    chrome_atom = portage_atom_string.splitlines()[-1].split('=')[1]
    for board in boards:
      keywords_file = CHROME_KEYWORDS_FILE % {'board': board}
      cros_lib.SudoRunCommand(
          ['mkdir', '-p', os.path.dirname(keywords_file)],
          enter_chroot=True, cwd=cwd)
      cros_lib.SudoRunCommand(
          ['tee', keywords_file], input='=%s\n' % chrome_atom,
          enter_chroot=True, cwd=cwd)
    return chrome_atom


def CleanupChromeKeywordsFile(boards, buildroot):
  """Cleans chrome uprev artifact if it exists."""
  for board in boards:
    keywords_path_in_chroot = CHROME_KEYWORDS_FILE % {'board': board}
    keywords_file = '%s/chroot%s' % (buildroot, keywords_path_in_chroot)
    if os.path.exists(keywords_file):
      cros_lib.SudoRunCommand(['rm', '-f', keywords_file])


def UprevPackages(buildroot, boards, overlays):
  """Uprevs non-browser chromium os packages that have changed."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  chroot_overlays = [
      cros_lib.ReinterpretPathForChroot(path) for path in overlays ]
  cros_lib.RunCommand(
      ['../../chromite/buildbot/cros_mark_as_stable', '--all',
       '--boards=%s' % ':'.join(boards),
       '--overlays=%s' % ':'.join(chroot_overlays),
       '--drop_file=%s' % cros_lib.ReinterpretPathForChroot(
           _PACKAGE_FILE % {'buildroot': buildroot}),
       'commit'], cwd=cwd, enter_chroot=True)


def UprevPush(buildroot, overlays, dryrun):
  """Pushes uprev changes to the main line."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['../../chromite/buildbot/cros_mark_as_stable',
         '--srcroot=%s' % os.path.join(buildroot, 'src'),
         '--overlays=%s' % ':'.join(overlays)
        ]
  if dryrun:
    cmd.append('--dryrun')

  cmd.append('push')
  cros_lib.RunCommand(cmd, cwd=cwd)


def AddPackagesForPrebuilt(filename):
  """Add list of packages for upload.

  Process a file that lists all the packages that can be uploaded to the
  package prebuilt bucket and generates the command line args for prebuilt.py.

  Args:
    filename: file with the package full name (category/name-version), one
              package per line.

  Returns:
    A list of parameters for prebuilt.py. For example:
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
    cros_lib.Warning('Problem with package file %s' % filename)
    cros_lib.Warning('Skipping uploading of prebuilts.')
    cros_lib.Warning('ERROR(%d): %s' % (errno, strerror))
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
    binhost_key: key parameter to pass onto prebuilt.py. If it equals None then
                 chrome_rev is used to select a default key.
    binhost_base_url: base url for prebuilt.py. If None the parameter
                      --binhost-base-url is absent.
    git_sync: boolean that enables --git-sync prebuilt.py parameter.
    use_binhost_package_file: use the File that contains the packages to upload
                              to the binhost. If it equals False then all
                              packages are selected.
    extra_args: Extra args to send to prebuilt.py.
  """
  if extra_args is None:
    extra_args = []

  cwd = os.path.dirname(os.path.realpath(__file__))
  cmd = ['./prebuilt.py',
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
  cros_lib.RunCommand(cmd, cwd=cwd)


def GenerateBreakpadSymbols(buildroot, board):
  """Generate breakpad symbols.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine
  """
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./cros_generate_breakpad_symbols', '--board=%s' % board]
  cros_lib.RunCommandCaptureOutput(cmd, cwd=cwd, enter_chroot=True)


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
  board_dir = os.path.join(buildroot, 'chroot', 'build', board, 'usr', 'lib')
  debug_tgz = os.path.join(archive_path, 'debug.tgz')
  cmd = ['tar', 'czf', debug_tgz]
  if gdb_symbols:
    cmd.extend(['--exclude', 'debug/usr/local/autotest',
                '--exclude', 'debug/tests',
                'debug'])
  else:
    cmd.append('debug/breakpad')

  tar_cmd = cros_lib.SudoRunCommand(cmd, cwd=board_dir, error_ok=True)

  # Emerging the factory kernel while this is running installs different debug
  # symbols. When tar spots this, it flags this and returns status code 1.
  # The tarball is still OK, although the kernel debug symbols might be garbled.
  # If tar fails in a different way, it'll return an error code other than 1.
  # TODO(davidjames): Remove factory kernel emerge from archive_build.
  if tar_cmd.returncode not in (0, 1):
    raise Exception('%r failed with exit code %s' % (cmd, tar_cmd.returncode))

  # Fix permissions and ownership on debug tarball.
  cros_lib.SudoRunCommand(['chown', str(os.getuid()), debug_tgz])
  os.chmod(debug_tgz, 0644)

  return os.path.basename(debug_tgz)


def UploadArchivedFile(archive_path, upload_url, filename, debug):
  """Upload the specified tarball from the archive dir to Google Storage.

  Args:
    archive_path: Path to archive dir.
    upload_url: Location where tarball should be uploaded.
    debug: Whether we are in debug mode.
  """

  if upload_url and not debug:
    full_filename = os.path.join(archive_path, filename)
    full_url = '%s/%s' % (upload_url, filename)
    cros_lib.RunCommandCaptureOutput([_GSUTIL_PATH, 'cp', full_filename,
                                      full_url])
    cros_lib.RunCommandCaptureOutput([_GSUTIL_PATH, 'setacl', _GS_ACL,
                                      full_url])


def UploadSymbols(buildroot, board, official):
  """Upload debug symbols for this build."""
  cmd = ['./upload_symbols',
        '--board=%s' % board,
        '--yes',
        '--verbose']

  if official:
    cmd += ['--official_build']

  cwd = os.path.join(buildroot, 'src', 'scripts')

  cros_lib.RunCommand(cmd, cwd=cwd, error_ok=True, enter_chroot=True)


def PushImages(buildroot, board, branch_name, archive_url, profile):
  """Push the generated image to http://chromeos_images."""
  cmd = ['./pushimage', '--board=%s' % board]

  if profile:
    cmd.append('--profile=%s' % profile)

  if branch_name:
    cmd.append('--branch=%s' % branch_name)

  cmd.append(archive_url)

  cros_lib.RunCommand(cmd, cwd=os.path.join(buildroot, 'crostools'))


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
  scripts_dir = os.path.join(buildroot, 'src', 'scripts')
  alias = _FACTORY_TEST
  cmd = ['./build_image',
         '--board=%s' % board,
         '--replace',
         '--noenable_rootfs_verification',
         '--symlink=%s' % alias,
         '--build_attempt=2',
         'factory_test']
  cros_lib.RunCommandCaptureOutput(cmd, enter_chroot=True, extra_env=extra_env,
                                   cwd=scripts_dir)
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
  scripts_dir = os.path.join(buildroot, 'src', 'scripts')
  alias = _FACTORY_SHIM
  cmd = ['./build_image',
         '--board=%s' % board,
         '--replace',
         '--symlink=%s' % alias,
         '--build_attempt=3',
         'factory_install']
  cros_lib.RunCommandCaptureOutput(cmd, enter_chroot=True, extra_env=extra_env,
                                   cwd=scripts_dir)
  return alias


def MakeNetboot(buildroot, board, image_dir):
  """Convert the specified image to be a netboot image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    image_dir: Directory containing factory install shim.
  """
  scripts_dir = os.path.join(buildroot, 'src', 'scripts')
  image = os.path.join(image_dir, 'factory_install_shim.bin')
  cmd = ['./make_netboot.sh',
         '--board=%s' % board,
         '--image=%s' % cros_lib.ReinterpretPathForChroot(image)]
  cros_lib.RunCommandCaptureOutput(cmd, enter_chroot=True, cwd=scripts_dir)


def BuildRecoveryImage(buildroot, board, image_dir, extra_env):
  """Build a recovery image.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    image_dir: Directory containing base image.
    extra_env: Flags to be added to the environment for the new process.
  """
  scripts_dir = os.path.join(buildroot, 'src', 'scripts')
  image = os.path.join(image_dir, 'chromiumos_base_image.bin')
  cmd = ['./mod_image_for_recovery.sh',
         '--board=%s' % board,
         '--image=%s' % cros_lib.ReinterpretPathForChroot(image)]
  cros_lib.RunCommandCaptureOutput(cmd, enter_chroot=True, extra_env=extra_env,
                                   cwd=scripts_dir)


def BuildTarball(buildroot, input_list, tarball_output, cwd=None):
  """Tars files and directories from input_list to tarball_output.

  Args:
    buildroot: Root directory where build occurs.
    input_list: A list of files and directories to be archived.
    tarball_output: Path of output tar archive file.
    cwd: Current working directory when tar command is executed.
  """
  pbzip2 = os.path.join(buildroot, 'chroot', 'usr', 'bin', 'pbzip2')
  cmd = ['tar',
         '--use-compress-program=%s' % pbzip2,
         '-cf', tarball_output]
  cmd += input_list
  cros_lib.RunCommandCaptureOutput(cmd, cwd=cwd)


def BuildAutotestTarballs(buildroot, board, tarball_dir):
  """Tar up the autotest artifacts into image_dir.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    tarball_dir: Location for storing autotest tarballs.

  Returns a tuple containing the paths of the autotest and test_suites tarballs.
  """
  autotest_tarball = os.path.join(tarball_dir, 'autotest.tar.bz2')
  test_suites_tarball = os.path.join(tarball_dir, 'test_suites.tar.bz2')
  cwd = os.path.join(buildroot, 'chroot', 'build', board, 'usr', 'local')

  BuildTarball(buildroot, ['autotest'], autotest_tarball, cwd=cwd)
  BuildTarball(buildroot, ['autotest/test_suites'], test_suites_tarball,
               cwd=cwd)
  return [autotest_tarball, test_suites_tarball]


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
  cros_lib.RunCommandCaptureOutput(['zip', zipfile, '-r', '.'], cwd=image_dir)
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
  files = ['image.bin', 'ec.bin']
  firmware_root = os.path.join(buildroot, 'chroot', 'build', board, 'firmware')
  source_list = [image_file
                 for image_file in files
                 if os.path.exists(os.path.join(firmware_root, image_file))]
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

  cros_lib.RunCommandCaptureOutput(cmd, cwd=temp_dir)
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
  cros_lib.RunCommandCaptureOutput(cmd)
  return '%s.tar.bz2' % hwqual_name


def UpdateLatestFile(bot_archive_root, set_version):
  """Update the latest file in archive_root.

  Args:
    bot_archive_root: Parent directory of archive directory.
    set_version: Version of output directory.
  """
  latest_path = os.path.join(bot_archive_root, 'LATEST')
  latest_file = open(latest_path, 'w')
  print >> latest_file, set_version
  latest_file.close()


def RemoveOldArchives(bot_archive_root, keep_max):
  """Remove old archive directories in bot_archive_root.

  Args:
    bot_archive_root: Parent directory containing old directories.
    keep_max: Maximum number of directories to keep.
  """
  # TODO(davidjames): Reimplement this in Python.
  # +2 because line numbers start at 1 and need to skip LATEST file
  cmd = 'ls -t1 | tail --lines=+%d | xargs rm -rf' % (keep_max + 2)
  cros_lib.RunCommandCaptureOutput(cmd, cwd=bot_archive_root, shell=True)


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
  cros_lib.RunCommandCaptureOutput(cmd)


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
  cros_lib.RunCommandCaptureOutput(cmd)
