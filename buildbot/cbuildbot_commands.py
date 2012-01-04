# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various individual commands a builder can run."""

import constants
import logging
import os
import re
import shutil
import tempfile

from chromite.buildbot import cbuildbot_background as background
from chromite.lib import cros_build_lib as cros_lib

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
_GS_ACL = '/home/chrome-bot/slave_archive_acl'
_BINHOST_PACKAGE_FILE = '/etc/portage/make.profile/package.installable'

class TestException(Exception):
  pass

# =========================== Command Helpers =================================

def _BuildRootGitCleanup(buildroot):
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

  def RunCleanupCommands(cwd):
    for check_returncode, cmds in tasks:
      for cmd in cmds:
        result = cros_lib.RunCommand(
            cmd, cwd=cwd, print_cmd=False, redirect_stdout=True,
            combine_stdout_stderr=True, error_code_ok=True)
        if check_returncode and result.returncode != 0:
          logging.info(result.output)
          logging.warn('Deleting %s because %s failed', cwd, cmd)
          shutil.rmtree(cwd)
          print '@@@STEP_WARNINGS@@@'
          return

  # Build list of directories to cleanup.
  result = cros_lib.RunCommand(['repo', 'list'], print_cmd=False,
                               redirect_stdout=True, cwd=buildroot)
  dirs = []
  for line in result.output.splitlines():
    subdir, _ = line.split(' ', 1)
    cwd = os.path.join(buildroot, subdir)
    if os.path.isdir(cwd):
      dirs.append([cwd])

  # Cleanup all of the directories.
  background.RunTasksInProcessPool(RunCleanupCommands, dirs)


def _CleanUpMountPoints(buildroot):
  """Cleans up any stale mount points from previous runs."""
  mount_output = cros_lib.RunCommand(['mount'], redirect_stdout=True,
                                     print_cmd=False)
  mount_pts_in_buildroot = cros_lib.RunCommand(
      ['grep', buildroot], input=mount_output.output, redirect_stdout=True,
      error_code_ok=True, print_cmd=False)

  for mount_pt_str in mount_pts_in_buildroot.output.splitlines():
    mount_pt = mount_pt_str.rpartition(' type ')[0].partition(' on ')[2]
    cros_lib.RunCommand(['sudo', 'umount', '-l', mount_pt], error_ok=True,
                        print_cmd=False)


def _GetVMConstants(buildroot):
  """Returns minimum (vdisk_size, statefulfs_size) recommended for VM's."""
  cwd = os.path.join(buildroot, 'src', 'scripts', 'lib')
  source_cmd = 'source %s/cros_vm_constants.sh' % cwd
  vdisk_size = cros_lib.RunCommand([
      '/bin/bash', '-c', '%s && echo $MIN_VDISK_SIZE_FULL' % source_cmd],
       redirect_stdout=True).output.strip()
  statefulfs_size = cros_lib.RunCommand([
      '/bin/bash', '-c', '%s && echo $MIN_STATEFUL_FS_SIZE_FULL' % source_cmd],
       redirect_stdout=True).output.strip()
  return (vdisk_size, statefulfs_size)


def _WipeOldOutput(buildroot):
  """Wipes out build output directories."""
  cros_lib.RunCommand(['rm', '-rf', 'src/build/images'], cwd=buildroot)


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


def PreFlightRinse(buildroot):
  """Cleans up any leftover state from previous runs."""
  _BuildRootGitCleanup(buildroot)
  _CleanUpMountPoints(buildroot)
  cros_lib.RunCommand(['sudo', 'killall', 'kvm'], error_ok=True)


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
  cmd = ['../../chromite/bin/refresh_package_status']

  # Skip the host board if present.
  board = ':'.join([b for b in boards if b != 'amd64-host'])
  cmd.append('--board=%s' % board)

  # Upload to the test spreadsheet only when in debug mode.
  if debug:
    cmd.append('--test-spreadsheet')

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True)


def SetupBoard(buildroot, board, fast, usepkg, latest_toolchain,
               extra_env=None, profile=None):
  """Wrapper around setup_board."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./setup_board', '--board=%s' % board]

  if profile:
    cmd.append('--profile=%s' % profile)

  if not usepkg:
    cmd.append('--nousepkg')

  if fast:
    cmd.append('--fast')
  else:
    cmd.append('--nofast')

  if latest_toolchain:
    cmd.append('--latest_toolchain')

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, extra_env=extra_env)


def Build(buildroot, board, build_autotest, fast, usepkg, skip_toolchain_update,
          nowithdebug, extra_env=None, chrome_root=None):
  """Wrapper around build_packages."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./build_packages', '--board=%s' % board]
  if extra_env is None:
    env = {}
  else:
    env = extra_env.copy()

  if fast:
    cmd.append('--fast')
  else:
    cmd.append('--nofast')

  if not build_autotest: cmd.append('--nowithautotest')

  if skip_toolchain_update: cmd.append('--skip_toolchain_update')

  if not usepkg:
    cmd.append('--nousepkg')

  if nowithdebug:
    cmd.append('--nowithdebug')

  chroot_args = []
  if chrome_root:
    chroot_args.append('--chrome_root=%s' % chrome_root)

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, extra_env=env,
                      chroot_args=chroot_args)


def BuildImage(buildroot, board, images_to_build, extra_env=None):
  _WipeOldOutput(buildroot)
  cwd = os.path.join(buildroot, 'src', 'scripts')
  # Default to base if images_to_build is passed empty.
  if not images_to_build: images_to_build = ['base']

  cmd = ['./build_image', '--board=%s' % board, '--replace'] + images_to_build
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
                 nplus1_archive_dir, build_config):
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
    cmd.append('--nplus1_archive_dir=%s' % nplus1_archive_dir)
    cmd.append('--build_config=%s' % build_config)

  result = cros_lib.RunCommand(cmd, cwd=cwd, error_ok=True, exit_code=True)
  if result.returncode:
    failed = open(results_dir_in_chroot + '/failed_test_command', 'w')
    failed.write('%s exited with code %d' % (' '.join(cmd), result.returncode))
    failed.close()
    raise TestException('** VMTests failed with code %d **'
      % result.returncode)


def UpdateRemoteHW(buildroot, image_dir, remote_ip):
  """Reimage the remote machine using the image modified for test."""

  cwd = os.path.join(buildroot, 'src', 'scripts')
  test_image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')
  cmd = ['./image_to_live.sh',
         '--remote=%s' % remote_ip,
         '--image=%s' % test_image_path, ]

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=False, error_ok=False,
                      print_cmd=True)


def RunRemoteTest(buildroot, board, remote_ip, test_name, args=None):
  """Execute an autotest on a remote machine."""

  cwd = os.path.join(buildroot, 'src', 'scripts')

  cmd = ['./run_remote_tests.sh',
         '--board=%s' % board,
         '--remote=%s' % remote_ip]

  if args and len(args) > 0:
    cmd.append('--args=%s' % ','.join(args))

  cmd.append(test_name)

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, error_ok=False,
                      print_cmd=True)


def ArchiveTestResults(buildroot, test_results_dir, prefix):
  """Archives the test results into a tarball.

  Arguments:
    buildroot: Root directory where build occurs.
    test_results_dir: Path from buildroot/chroot to find test results.
      This must a subdir of /tmp.

  Returns the path to the tarball.
  """
  try:
    test_results_dir = test_results_dir.lstrip('/')
    results_path = os.path.join(buildroot, 'chroot', test_results_dir)
    cros_lib.RunCommand(['sudo', 'chmod', '-R', 'a+rw', results_path],
                        print_cmd=False)

    test_tarball = os.path.join(buildroot, '%stest_results.tgz' % prefix)
    if os.path.exists(test_tarball): os.remove(test_tarball)
    cros_lib.RunCommand(['tar', 'czf', test_tarball,
                         '--directory=%s' % results_path,'.'],
                        print_cmd=False)
    shutil.rmtree(results_path)

    return test_tarball

  except Exception, e:
    cros_lib.Warning('========================================================')
    cros_lib.Warning('------>  We failed to archive test results. <-----------')
    cros_lib.Warning(str(e))
    cros_lib.Warning('========================================================')


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
                                exit_code=True,
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
                       board,
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
             '--board=%s' % board, ]
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
    keywords_file = CHROME_KEYWORDS_FILE % {'board': board}
    cros_lib.RunCommand(
        ['sudo', 'mkdir', '-p', os.path.dirname(keywords_file)],
        enter_chroot=True, cwd=cwd)
    cros_lib.RunCommand(
        ['sudo', 'tee', keywords_file], input='=%s\n' % chrome_atom,
        enter_chroot=True, cwd=cwd)
    return chrome_atom


def CleanupChromeKeywordsFile(board, buildroot):
  """Cleans chrome uprev artifact if it exists."""
  keywords_path_in_chroot = CHROME_KEYWORDS_FILE % {'board': board}
  keywords_file = '%s/chroot%s' % (buildroot, keywords_path_in_chroot)
  if os.path.exists(keywords_file):
    cros_lib.RunCommand(['sudo', 'rm', '-f', keywords_file])


def UprevPackages(buildroot, board, overlays):
  """Uprevs non-browser chromium os packages that have changed."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  chroot_overlays = [
      cros_lib.ReinterpretPathForChroot(path) for path in overlays ]
  cros_lib.RunCommand(
      ['../../chromite/buildbot/cros_mark_as_stable', '--all',
       '--board=%s' % board,
       '--overlays=%s' % ':'.join(chroot_overlays),
       '--drop_file=%s' % cros_lib.ReinterpretPathForChroot(
           _PACKAGE_FILE % {'buildroot': buildroot}),
       'commit'], cwd=cwd, enter_chroot=True)


def UprevPush(buildroot, board, overlays, dryrun):
  """Pushes uprev changes to the main line."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['../../chromite/buildbot/cros_mark_as_stable',
         '--srcroot=%s' % os.path.join(buildroot, 'src'),
         '--board=%s' % board,
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


def UploadPrebuilts(buildroot, board, overlay_config, category,
                    chrome_rev, buildnumber,
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
    overlay_config: A string describing which overlays you want.
                    'private': Just the private overlay.
                    'public': Just the public overlay.
                    'both': Both the public and private overlays.
    category: Build type. Can be [binary|full|chrome].
    chrome_rev: Chrome_rev of type constants.VALID_CHROME_REVISIONS.
    buildnumber:  self explanatory.
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

  cwd = os.path.dirname(__file__)
  cmd = ['./prebuilt.py',
         '--build-path', buildroot,
         '--prepend-version', category]

  if binhost_base_url is not None:
    cmd.extend(['--binhost-base-url', binhost_base_url])

  if binhost_bucket is not None:
    cmd.extend(['--upload', binhost_bucket])
  else:
    cmd.extend(['--upload', 'gs://chromeos-prebuilt'])

  if overlay_config in ('private', 'both'):
    cmd.extend(['--private', '--binhost-conf-dir', _PRIVATE_BINHOST_CONF_DIR])

  if category == 'chroot':
    cmd.extend(['--sync-host',
                '--board', 'amd64-host',
                '--upload-board-tarball'])
  else:
    cmd.extend(['--board', board])

  if binhost_key is not None:
    cmd.extend(['--key', binhost_key])
  elif category == constants.CHROME_PFQ_TYPE:
    assert chrome_rev
    key = '%s_%s' % (chrome_rev, _CHROME_BINHOST)
    cmd.extend(['--key', key.upper()])
  elif category == constants.PFQ_TYPE:
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
  cmd = ['./cros_generate_breakpad_symbols',
         '--board=%s' % board]
  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True)


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
  cmd = ['sudo', 'tar', 'czf', debug_tgz, '--checkpoint=10000']
  if gdb_symbols:
    cmd.extend(['--exclude', 'debug/usr/local/autotest',
                '--exclude', 'debug/tests',
                'debug'])
  else:
    cmd.append('debug/breakpad')

  tar_cmd = cros_lib.RunCommand(cmd, cwd=board_dir, error_ok=True,
                                exit_code=True)

  # Emerging the factory kernel while this is running installs different debug
  # symbols. When tar spots this, it flags this and returns status code 1.
  # The tarball is still OK, although the kernel debug symbols might be garbled.
  # If tar fails in a different way, it'll return an error code other than 1.
  # TODO(davidjames): Remove factory kernel emerge from archive_build.
  if tar_cmd.returncode not in (0, 1):
    raise Exception('%r failed with exit code %s' % (cmd, tar_cmd.returncode))

  # Fix permissions and ownership on debug tarball.
  cros_lib.RunCommand(['sudo', 'chown', str(os.getuid()), debug_tgz])
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
    cros_lib.RunCommand([_GSUTIL_PATH,
                         'cp',
                         full_filename,
                         full_url])
    cros_lib.RunCommand([_GSUTIL_PATH,
                         'setacl',
                         _GS_ACL,
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
  cros_lib.RunCommand(cmd, enter_chroot=True, extra_env=extra_env,
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
  cros_lib.RunCommand(cmd, enter_chroot=True, extra_env=extra_env,
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
  cros_lib.RunCommand(cmd, enter_chroot=True, cwd=scripts_dir)


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
  cros_lib.RunCommand(cmd, enter_chroot=True, extra_env=extra_env,
                      cwd=scripts_dir)


def BuildAutotestTarball(buildroot, board, filename):
  """Tar up the autotest artifacts into image_dir.

  Args:
    buildroot: Root directory where build occurs.
    board: Board type that was built on this machine.
    filename: Location for storing autotest tarball

  Returns the basename of the autotest tarball.
  """
  cwd = os.path.join(buildroot, 'chroot', 'build', board, 'usr', 'local')
  pbzip2 = os.path.join(buildroot, 'chroot', 'usr', 'bin', 'pbzip2')
  cmd = ['tar',
         'cf',
         filename,
         '--checkpoint=10000',
         '--use-compress-program=%s' % pbzip2,
         'autotest']
  cros_lib.RunCommand(cmd, cwd=cwd)


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
  cmd = ['zip', zipfile, '-r', '.']
  cros_lib.RunCommand(cmd, cwd=image_dir)
  return filename


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

  cros_lib.RunCommand(cmd, cwd=temp_dir)
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
  cros_lib.RunCommand(cmd)
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
  cros_lib.RunCommand(cmd, cwd=bot_archive_root, shell=True)


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
