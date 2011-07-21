# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various individual commands a builder can run."""

import constants
import os
import re
import shutil
import sys

from chromite.buildbot import cbuildbot_config
from chromite.buildbot import repository
from chromite.lib import cros_build_lib as cros_lib


_DEFAULT_RETRIES = 3
_PACKAGE_FILE = '%(buildroot)s/src/scripts/cbuildbot_package.list'
CHROME_KEYWORDS_FILE = ('/build/%(board)s/etc/portage/package.keywords/chrome')
_PREFLIGHT_BINHOST = 'PREFLIGHT_BINHOST'
_CHROME_BINHOST = 'CHROME_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'
_FULL_BINHOST = 'FULL_BINHOST'
_PRIVATE_BINHOST_CONF_DIR = ('src/private-overlays/chromeos-overlay/'
                             'chromeos/binhost')
_GSUTIL_PATH = '/b/scripts/slave/gsutil'
_GS_GEN_INDEX = '/b/scripts/gsd_generate_index/gsd_generate_index.py'
_GS_ACL = '/home/chrome-bot/slave_archive_acl'


# =========================== Command Helpers =================================


def _BuildRootGitCleanup(buildroot):
  """Put buildroot onto manifest branch. Delete branches created on last run."""
  manifest_branch = 'remotes/m/' + cros_lib.GetManifestDefaultBranch(buildroot)
  project_list = cros_lib.RunCommand(['repo', 'forall', '-c', 'pwd'],
                                     redirect_stdout=True,
                                     cwd=buildroot).output.splitlines()
  for project in project_list:
    # The 'git clean' command below might remove some repositories.
    if not os.path.exists(project):
      continue

    cros_lib.RunCommand(['git', 'am', '--abort'], print_cmd=False,
                        redirect_stdout=True, redirect_stderr=True,
                        error_ok=True, cwd=project)
    cros_lib.RunCommand(['git', 'rebase', '--abort'], print_cmd=False,
                        redirect_stdout=True, redirect_stderr=True,
                        error_ok=True, cwd=project)
    cros_lib.RunCommand(['git', 'reset', '--hard', 'HEAD'], print_cmd=False,
                        redirect_stdout=True, cwd=project)
    cros_lib.RunCommand(['git', 'checkout', manifest_branch], print_cmd=False,
                        redirect_stdout=True, redirect_stderr=True,
                        cwd=project)
    cros_lib.RunCommand(['git', 'clean', '-f', '-d'], print_cmd=False,
                        redirect_stdout=True, cwd=project)

    for branch in constants.CREATED_BRANCHES:
      if cros_lib.DoesLocalBranchExist(project, branch):
        cros_lib.RunCommand(['repo', 'abandon', branch, '.'], cwd=project)


def _CleanUpMountPoints(buildroot):
  """Cleans up any stale mount points from previous runs."""
  mount_output = cros_lib.OldRunCommand(['mount'], redirect_stdout=True,
                                        print_cmd=False)
  mount_pts_in_buildroot = cros_lib.OldRunCommand(
      ['grep', buildroot], input=mount_output, redirect_stdout=True,
      error_ok=True, print_cmd=False)

  for mount_pt_str in mount_pts_in_buildroot.splitlines():
    mount_pt = mount_pt_str.rpartition(' type ')[0].partition(' on ')[2]
    cros_lib.OldRunCommand(['sudo', 'umount', '-l', mount_pt], error_ok=True,
                           print_cmd=False)


def _GetVMConstants(buildroot):
  """Returns minimum (vdisk_size, statefulfs_size) recommended for VM's."""
  cwd = os.path.join(buildroot, 'src', 'scripts', 'lib')
  source_cmd = 'source %s/cros_vm_constants.sh' % cwd
  vdisk_size = cros_lib.OldRunCommand([
      '/bin/bash', '-c', '%s && echo $MIN_VDISK_SIZE_FULL' % source_cmd],
       redirect_stdout=True)
  statefulfs_size = cros_lib.OldRunCommand([
      '/bin/bash', '-c', '%s && echo $MIN_STATEFUL_FS_SIZE_FULL' % source_cmd],
       redirect_stdout=True)
  return (vdisk_size.strip(), statefulfs_size.strip())


def _WipeOldOutput(buildroot):
  """Wipes out build output directories."""
  cros_lib.OldRunCommand(['rm', '-rf', 'src/build/images'], cwd=buildroot)


# =========================== Main Commands ===================================


def PreFlightRinse(buildroot):
  """Cleans up any leftover state from previous runs."""
  _BuildRootGitCleanup(buildroot)
  _CleanUpMountPoints(buildroot)
  cros_lib.OldRunCommand(['sudo', 'killall', 'kvm'], error_ok=True)


def ManifestCheckout(buildroot, tracking_branch, next_manifest, url):
  """Performs a manifest checkout and clobbers any previous checkouts."""

  print "BUILDROOT: %s" % buildroot
  print "TRACKING BRANCH: %s" % tracking_branch
  print "NEXT MANIFEST: %s" % next_manifest

  repo = repository.RepoRepository(url, buildroot, branch=tracking_branch)
  repo.Sync(next_manifest)
  repo.ExportManifest('/dev/stderr')


def MakeChroot(buildroot, replace, fast, usepkg):
  """Wrapper around make_chroot."""
  # TODO(zbehan): Remove this hack. crosbug.com/17474
  if os.environ.get('USE_CROS_SDK') == '1':
    # We assume these two are on for cros_sdk. Fail out if they aren't.
    assert usepkg
    assert fast
    cwd = os.path.join(buildroot, 'chromite', 'bin')
    cmd = ['./cros_sdk']
  else:
    cwd = os.path.join(buildroot, 'src', 'scripts')
    cmd = ['./make_chroot']

    if not usepkg:
      cmd.append('--nousepkg')

    if fast:
      cmd.append('--fast')
    else:
      cmd.append('--nofast')

  if replace:
    cmd.append('--replace')

  cros_lib.OldRunCommand(cmd, cwd=cwd)


def RunChrootUpgradeHooks(buildroot):
  """Run the chroot upgrade hooks in the chroot."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cros_lib.RunCommand(['./run_chroot_version_hooks'], cwd=cwd,
                      enter_chroot=True)


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
          extra_env=None):
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

  if usepkg:
    key = 'EXTRA_BOARD_FLAGS'
    prev = env.get(key)
    env[key] = (prev and prev + ' ' or '') + '--rebuilt-binaries'
  else:
    cmd.append('--nousepkg')

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, extra_env=env)


def BuildImage(buildroot, board, mod_for_test, extra_env=None):
  _WipeOldOutput(buildroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./build_image', '--board=%s' % board, '--replace']
  if mod_for_test:
    cmd.append('--test')

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


def RunUnitTests(buildroot, board, full):
  cwd = os.path.join(buildroot, 'src', 'scripts')

  cmd = ['cros_run_unit_tests', '--board=%s' % board]

  # If we aren't running ALL tests, then restrict to just the packages
  #   uprev noticed were changed.
  if not full:
    cmd += ['--package_file=%s' %
            cros_lib.ReinterpretPathForChroot(_PACKAGE_FILE %
                                              {'buildroot': buildroot})]

  cros_lib.OldRunCommand(cmd, cwd=cwd, enter_chroot=True)


def RunChromeSuite(buildroot, board, image_dir, results_dir):
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  if os.path.exists(results_dir_in_chroot):
    shutil.rmtree(results_dir_in_chroot)

  image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')

  cwd = os.path.join(buildroot, 'src', 'scripts')
  # TODO(cmasone): make this look for ALL desktopui_BrowserTest control files.
  cros_lib.OldRunCommand(['bin/cros_run_parallel_vm_tests',
                          '--board=%s' % board,
                          '--quiet',
                          '--image_path=%s' % image_path,
                          '--results_dir_root=%s' % results_dir,
                          'desktopui_BrowserTest.control$',
                          'desktopui_BrowserTest.control.one',
                          'desktopui_BrowserTest.control.two',
                          'desktopui_BrowserTest.control.three',
                         ], cwd=cwd, error_ok=True, enter_chroot=False)


def RunTestSuite(buildroot, board, image_dir, results_dir, full=True):
  """Runs the test harness suite."""
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  if os.path.exists(results_dir_in_chroot):
    shutil.rmtree(results_dir_in_chroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')

  if full:
    cmd = ['bin/ctest',
           '--board=%s' % board,
           '--channel=dev-channel',
           '--zipbase=http://chromeos-images.corp.google.com',
           '--type=vm',
           '--no_graphics',
           '--test_results_root=%s' % results_dir_in_chroot, ]
  else:
    cmd = ['bin/cros_au_test_harness',
           '--no_graphics',
           '--no_delta',
           '--board=%s' % board,
           '--test_prefix=SimpleTest',
           '--verbose',
           '--base_image=%s' % image_path,
           '--target_image=%s' % image_path,
           '--test_results_root=%s' % results_dir_in_chroot, ]

  cros_lib.OldRunCommand(cmd, cwd=cwd, error_ok=False)


def UpdateRemoteHW(buildroot, board, image_dir, remote_ip):
  """Reimage the remote machine using the image modified for test."""

  cwd = os.path.join(buildroot, 'src', 'scripts')
  test_image_path = os.path.join(image_dir, 'chromiumos_test_image.bin')
  cmd = ['./image_to_live.sh',
         '--remote=%s' % remote_ip,
         '--image=%s' % test_image_path, ]

  cros_lib.OldRunCommand(cmd, cwd=cwd, enter_chroot=False, error_ok=False,
                         print_cmd=True)


def RemoteRunPyAuto(buildroot, board, remote_ip, internal_test):
  """Execute PyAuto tests on a remote machine.

  Runs the CONTINUOUS suite of desktopui_PyAutoFunctionalTests on a remote
  Chromium OS device.
  """

  cwd = os.path.join(buildroot, 'src', 'scripts')
  test_suite = 'client/site_tests/desktopui_PyAutoFunctionalTests/control'

  if internal_test:
    suite = 'CONTINUOUS'
  else:
    suite = 'CONTINUOUS_PUBLIC'

  cmd = ['./run_remote_tests.sh',
         '--board=%s' % board,
         '--remote=%s' % remote_ip,
         '--args=%s' % suite,
         test_suite, ]

  cros_lib.OldRunCommand(cmd, cwd=cwd, enter_chroot=True, error_ok=False,
                         print_cmd=True)


def ArchiveTestResults(buildroot, test_results_dir, upload_url, debug):
  """Archives the test results into a tarball and uploads it.

  Arguments:
    buildroot: Root directory where build occurs
    test_results_dir: Path from buildroot/chroot to find test results.
      This must a subdir of /tmp.
    upload_url: Google Storage location for test tarball.
  """
  try:
    test_results_dir = test_results_dir.lstrip('/')
    results_path = os.path.join(buildroot, 'chroot', test_results_dir)
    cros_lib.OldRunCommand(['sudo', 'chmod', '-R', 'a+rw', results_path],
                           print_cmd=False)

    archive_tarball = os.path.join(buildroot, 'test_results.tgz')
    if os.path.exists(archive_tarball): os.remove(archive_tarball)
    cros_lib.OldRunCommand(['tar',
                            'czf',
                            archive_tarball,
                            '--directory=%s' % results_path,
                            '.'])
    shutil.rmtree(results_path)

    if upload_url and not debug:
      tarball_url = '%s/%s' % (upload_url, 'test_results.tgz')
      cros_lib.OldRunCommand([_GSUTIL_PATH,
                              'cp',
                              archive_tarball,
                              tarball_url])

  except Exception, e:
    cros_lib.Warning('========================================================')
    cros_lib.Warning('------>  We failed to archive test results. <-----------')
    cros_lib.Warning(str(e))
    cros_lib.Warning('========================================================')


def MarkChromeAsStable(buildroot, tracking_branch, chrome_rev, board):
  """Returns the portage atom for the revved chrome ebuild - see man emerge."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  portage_atom_string = cros_lib.OldRunCommand(
      ['../../chromite/buildbot/cros_mark_chrome_as_stable',
       '--tracking_branch=%s' % tracking_branch,
       '--board=%s' % board,
       chrome_rev],
      cwd=cwd, redirect_stdout=True, enter_chroot=True).rstrip()
  if not portage_atom_string:
    cros_lib.Info('Found nothing to rev.')
    return None
  else:
    chrome_atom = portage_atom_string.splitlines()[-1].split('=')[1]
    keywords_file = CHROME_KEYWORDS_FILE % {'board': board}
    cros_lib.OldRunCommand(
        ['sudo', 'mkdir', '-p', os.path.dirname(keywords_file)],
        enter_chroot=True, cwd=cwd)
    cros_lib.OldRunCommand(
        ['sudo', 'tee', keywords_file], input='=%s\n' % chrome_atom,
        enter_chroot=True, cwd=cwd)
    return chrome_atom


def UprevPackages(buildroot, board, overlays):
  """Uprevs non-browser chromium os packages that have changed."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  chroot_overlays = [
      cros_lib.ReinterpretPathForChroot(path) for path in overlays ]
  cros_lib.OldRunCommand(
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
  cros_lib.OldRunCommand(cmd, cwd=cwd)


def UploadPrebuilts(buildroot, board, overlay_config, category,
                    chrome_rev, buildnumber, extra_args=[]):
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
    extra_args: Extra args to send to prebuilt.py.
  """
  cwd = os.path.dirname(__file__)
  cmd = ['./prebuilt.py',
         '--build-path', buildroot,
         '--prepend-version', category]
  if overlay_config == 'public':
    cmd.extend(['--upload', 'gs://chromeos-prebuilt'])
  else:
    assert overlay_config in ('private', 'both')
    upload_bucket = 'chromeos-%s' % board
    cmd.extend(['--upload', 'gs://%s/%s/%d/prebuilts/' %
                    (upload_bucket, category, buildnumber),
                '--private',
                '--binhost-conf-dir', _PRIVATE_BINHOST_CONF_DIR
               ])

  if category == 'chroot':
    cmd.extend(['--sync-host',
                '--board', 'amd64-host',
                '--upload-board-tarball'])
  else:
    cmd.extend(['--board', board])

  if category == constants.CHROME_PFQ_TYPE:
    assert chrome_rev
    key = '%s_%s' % (chrome_rev, _CHROME_BINHOST)
    cmd.extend(['--packages=chromeos-chrome',
                '--key', key.upper()])
  elif category == constants.PFQ_TYPE:
    cmd.extend(['--key', _PREFLIGHT_BINHOST])
  else:
    assert category in (constants.BUILD_FROM_SOURCE_TYPE,
                        constants.CHROOT_BUILDER_TYPE)
    # Commit new binhost directly to overlay.
    cmd.extend(['--git-sync',
                '--key', _FULL_BINHOST])
  cmd.extend(extra_args)

  cros_lib.OldRunCommand(cmd, cwd=cwd)


def LegacyArchiveBuild(buildroot, bot_id, buildconfig, gsutil_archive,
                       set_version, archive_path, debug=False):
  """Archives build artifacts and returns URL to archived location."""

  # Fixed properties
  keep_max = 3

  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./archive_build.sh',
         '--set_version', str(set_version),
         '--to', os.path.join(archive_path, bot_id),
         '--keep_max', str(keep_max),
         '--board', buildconfig['board'],
         ]

  # If we archive to Google Storage
  if gsutil_archive:
    cmd += ['--gsutil_archive', gsutil_archive,
            '--acl', _GS_ACL,
            '--gsutil', _GSUTIL_PATH,
            ]

  # Give the right args to archive_build.
  if buildconfig.get('chromeos_official'): cmd.append('--official_build')
  if buildconfig.get('factory_test_mod', True): cmd.append('--factory_test_mod')
  if not buildconfig['archive_build_debug']: cmd.append('--noarchive_debug')
  if not buildconfig.get('test_mod'): cmd.append('--notest_mod')
  if debug: cmd.append('--debug')
  if buildconfig.get('factory_install_mod', True):
    cmd.append('--factory_install_mod')

  useflags = buildconfig.get('useflags')
  if useflags: cmd.extend(['--useflags', ' '.join(useflags)])

  result = None
  try:
    # Files created in our archive dir should be publically accessable.
    old_umask = os.umask(022)
    cros_lib.RunCommand(cmd, cwd=cwd)
  finally:
    os.umask(old_umask)

def UpdateIndex(gsutil_archive, set_version):
  """Update _index.html page in Google Storage."""
  cros_lib.RunCommand([_GS_GEN_INDEX,
                       '--gsutil', _GSUTIL_PATH,
                       '--acl', _GS_ACL,
                       '-p', set_version,
                       gsutil_archive])


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


def PushImages(buildroot, board, branch_name, archive_dir):
  """Push the generated image to http://chromeos_images."""
  cmd = ['./pushimage',
        '--board=%s' % board,
        '--branch=%s' % branch_name,
        archive_dir]

  cros_lib.RunCommand(cmd, cwd=os.path.join(buildroot, 'crostools'))
