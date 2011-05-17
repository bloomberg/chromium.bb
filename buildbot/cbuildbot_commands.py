# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the various individual commands a builder can run."""

import os
import re
import shutil
import socket

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

# =========================== Command Helpers =================================

def _GitCleanup(buildroot, board, tracking_branch, overlays):
  """Clean up git branch after previous uprev attempt."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  if os.path.exists(cwd):
    cros_lib.OldRunCommand(
        ['../../chromite/buildbot/cros_mark_as_stable', '--srcroot=..',
         '--board=%s' % board,
         '--overlays=%s' % ':'.join(overlays),
         '--tracking_branch=%s' % tracking_branch, 'clean'
        ], cwd=cwd, error_ok=True)


def _CleanUpMountPoints(buildroot):
  """Cleans up any stale mount points from previous runs."""
  mount_output = cros_lib.OldRunCommand(['mount'], redirect_stdout=True)
  mount_pts_in_buildroot = cros_lib.OldRunCommand(
      ['grep', buildroot], input=mount_output, redirect_stdout=True,
      error_ok=True)

  for mount_pt_str in mount_pts_in_buildroot.splitlines():
    mount_pt = mount_pt_str.rpartition(' type ')[0].partition(' on ')[2]
    cros_lib.OldRunCommand(['sudo', 'umount', '-l', mount_pt], error_ok=True)


def _RepoSync(buildroot, retries=_DEFAULT_RETRIES):
  """Uses repo to checkout the source code.

  Keyword arguments:
  retries -- Number of retries to try before failing on the sync.
  """
  while retries > 0:
    try:
      cros_lib.OldRunCommand(['repo', 'sync', '-q', '--jobs=4'], cwd=buildroot)
      cros_lib.OldRunCommand(
          ['repo',
           'forall',
           '-c',
           'git',
           'config',
           'url.ssh://gerrit.chromium.org:29418.insteadof',
           'http://git.chromium.org'
          ], cwd=buildroot)
      retries = 0
    except:
      retries -= 1
      if retries > 0:
        cros_lib.Warning('CBUILDBOT -- Repo Sync Failed, retrying')
      else:
        cros_lib.Warning('CBUILDBOT -- Retries exhausted')
        raise

  # repo manifest uses PAGER, but we want it to be non-interactive
  os.environ['PAGER'] = 'cat'
  cros_lib.OldRunCommand(['repo', 'manifest', '-r', '-o', '/dev/stderr'],
                         cwd=buildroot)


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

def PreFlightRinse(buildroot, board, tracking_branch, overlays):
  """Cleans up any leftover state from previous runs."""
  _GitCleanup(buildroot, board, tracking_branch, overlays)
  _CleanUpMountPoints(buildroot)
  cros_lib.OldRunCommand(['sudo', 'killall', 'kvm'], error_ok=True)


def ManifestCheckout(buildroot, tracking_branch, next_version,
                     retries=_DEFAULT_RETRIES, url=None, mirror=None):
  """Performs a manifest checkout and clobbers any previous checkouts."""

  print "BUILDROOT: %s" % buildroot
  print "TRACKING BRANCH: %s" % tracking_branch
  print "NEXT VERSION: %s" % next_version
  print "URL: %s" % url

  branch = 'master'
  branch_parts = tracking_branch.split('/')
  if len(branch_parts) >= 2:
    branch = branch_parts[1]

  next_version_subdir = next_version.split('.');
  manifest = os.path.join('buildspecs',
                          next_version_subdir[0] + '.' + next_version_subdir[1],
                          next_version + '.xml')

  repository.RepoRepository(url, buildroot, branch=branch, manifest=manifest,
                            local_mirror=mirror, clobber=True).Sync()


def FullCheckout(buildroot, tracking_branch,
                 retries=_DEFAULT_RETRIES,
                 url='http://git.chromium.org/git/manifest'):
  """Performs a full checkout and clobbers any previous checkouts."""
  _CleanUpMountPoints(buildroot)
  cros_lib.OldRunCommand(['sudo', 'rm', '-rf', buildroot])
  os.makedirs(buildroot)
  branch = tracking_branch.split('/');
  cros_lib.OldRunCommand(['repo', 'init', '-q', '-u', url, '-b',
                         '%s' % branch[-1]], cwd=buildroot, input='\n\ny\n')
  _RepoSync(buildroot, retries)


def IncrementalCheckout(buildroot, retries=_DEFAULT_RETRIES):
  """Performs a checkout without clobbering previous checkout."""
  _RepoSync(buildroot, retries)


def MakeChroot(buildroot, replace=False):
  """Wrapper around make_chroot."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./make_chroot', '--fast']
  if replace:
    cmd.append('--replace')

  cros_lib.OldRunCommand(cmd, cwd=cwd)


def SetupBoard(buildroot, board='x86-generic'):
  """Wrapper around setup_board."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cros_lib.OldRunCommand(
      ['./setup_board', '--fast', '--default', '--board=%s' % board], cwd=cwd,
      enter_chroot=True)


def Build(buildroot, emptytree, build_autotest=True, usepkg=True,
          extra_env=None):
  """Wrapper around build_packages."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./build_packages']
  if extra_env is None:
    env = {}
  else:
    env = extra_env.copy()

  if not build_autotest: cmd.append('--nowithautotest')
  if not usepkg: cmd.append('--nousepkg')
  if emptytree:
    key = 'EXTRA_BOARD_FLAGS'
    prev = env.get(key)
    env[key] = (prev and prev + ' ' or '') + '--emptytree'

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True, extra_env=env)


def BuildImage(buildroot, extra_env=None):
  _WipeOldOutput(buildroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  cros_lib.RunCommand(['./build_image', '--replace'], cwd=cwd,
                         enter_chroot=True, extra_env=extra_env)


def BuildVMImageForTesting(buildroot, extra_env=None):
  (vdisk_size, statefulfs_size) = _GetVMConstants(buildroot)
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cros_lib.RunCommand(['./image_to_vm.sh',
                       '--test_image',
                       '--full',
                       '--vdisk_size=%s' % vdisk_size,
                       '--statefulfs_size=%s' % statefulfs_size,
                      ], cwd=cwd, enter_chroot=True, extra_env=extra_env)


def RunUnitTests(buildroot, full):
  cwd = os.path.join(buildroot, 'src', 'scripts')

  cmd = ['cros_run_unit_tests']

  # If we aren't running ALL tests, then restrict to just the packages
  #   uprev noticed were changed.
  if not full:
    cmd += ['--package_file=%s' %
            cros_lib.ReinterpretPathForChroot(_PACKAGE_FILE %
                                              {'buildroot': buildroot})]

  cros_lib.OldRunCommand(cmd, cwd=cwd, enter_chroot=True)


def RunChromeSuite(buildroot, results_dir):
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  if os.path.exists(results_dir_in_chroot):
    shutil.rmtree(results_dir_in_chroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  # TODO(cmasone): make this look for ALL desktopui_BrowserTest control files.
  cros_lib.OldRunCommand(['bin/cros_run_parallel_vm_tests',
                          '--quiet',
                          '--results_dir_root=%s' % results_dir,
                          'desktopui_BrowserTest.control$',
                          'desktopui_BrowserTest.control.one',
                          'desktopui_BrowserTest.control.two',
                          'desktopui_BrowserTest.control.three',
                         ], cwd=cwd, error_ok=True, enter_chroot=False)


def RunTestSuite(buildroot, board, results_dir, full=True):
  """Runs the test harness suite."""
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  if os.path.exists(results_dir_in_chroot):
    shutil.rmtree(results_dir_in_chroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  image_path = os.path.join(buildroot, 'src', 'build', 'images', board,
                            'latest', 'chromiumos_test_image.bin')

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


def ArchiveTestResults(buildroot, test_results_dir):
  """Archives the test results into a tarball and returns a path to it.

  Arguments:
    buildroot: Root directory where build occurs
    test_results_dir: Path from buildroot/chroot to find test results.
      This must a subdir of /tmp.
  Returns:
    Path to the newly archived test results.
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
    return archive_tarball
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


def UprevPackages(buildroot, tracking_branch, board, overlays):
  """Uprevs non-browser chromium os packages that have changed."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  chroot_overlays = [
      cros_lib.ReinterpretPathForChroot(path) for path in overlays ]
  cros_lib.OldRunCommand(
      ['../../chromite/buildbot/cros_mark_as_stable', '--all',
       '--board=%s' % board,
       '--overlays=%s' % ':'.join(chroot_overlays),
       '--tracking_branch=%s' % tracking_branch,
       '--drop_file=%s' % cros_lib.ReinterpretPathForChroot(
           _PACKAGE_FILE % {'buildroot': buildroot}),
       'commit'], cwd=cwd, enter_chroot=True)


def UprevPush(buildroot, tracking_branch, board, overlays, dryrun):
  """Pushes uprev changes to the main line."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['../../chromite/buildbot/cros_mark_as_stable',
         '--srcroot=%s' % os.path.join(buildroot, 'src'),
         '--board=%s' % board,
         '--overlays=%s' % ':'.join(overlays),
         '--tracking_branch=%s' % tracking_branch
        ]
  if dryrun:
    cmd.append('--dryrun')

  cmd.append('push')
  cros_lib.OldRunCommand(cmd, cwd=cwd)


def UploadPrebuilts(buildroot, board, overlay_config, binhosts, category,
                    chrome_rev):
  """Upload prebuilts.

  Args:
    buildroot: The root directory where the build occurs.
    board: Board type that was built on this machine
    overlay_config: A string describing which overlays you want.
                    'private': Just the private overlay.
                    'public': Just the public overlay.
                    'both': Both the public and private overlays.
    binhosts: The URLs of the current binhosts. Binaries that are already
              present will not be uploaded twice. Empty URLs will be ignored.
    category: Build type. Can be [binary|full|chrome].
    chrome_rev: Chrome_rev of type [tot|latest_release|sticky_release].
  """
  cwd = os.path.dirname(__file__)
  cmd = ['./prebuilt.py',
         '--build-path', buildroot,
         '--board', board,
         '--prepend-version', category]
  for binhost in binhosts:
    if binhost:
      cmd.extend(['--previous-binhost-url', binhost])
  if overlay_config == 'public':
    cmd.extend(['--upload', 'gs://chromeos-prebuilt'])
  else:
    assert overlay_config in ('private', 'both')
    cmd.extend(['--upload', 'chromeos-images:/var/www/prebuilt/',
                '--binhost-base-url', 'http://chromeos-prebuilt'])

  if category == 'chrome':
    assert chrome_rev
    key = '%s_%s' % (chrome_rev, _CHROME_BINHOST)
    cmd.extend(['--sync-binhost-conf',
                 '--key', key.upper()])
  elif category == 'binary':
    cmd.extend(['--sync-binhost-conf',
                '--key', _PREFLIGHT_BINHOST])
  else:
    assert category == 'full'
    # Commit new binhost directly to board overlay.
    cmd.extend(['--git-sync',
                '--key', _FULL_BINHOST])
    # Only one bot should upload full host prebuilts. We've arbitrarily
    # designated the x86-generic bot as the bot that does that.
    if board == 'x86-generic':
      cmd.append('--sync-host')


  cros_lib.OldRunCommand(cmd, cwd=cwd)


def LegacyArchiveBuild(buildroot, bot_id, buildconfig, buildnumber,
                        test_tarball, debug=False):
  """Archives build artifacts and returns URL to archived location."""

  # Fixed properties
  keep_max = 3

  if buildconfig['gs_path'] == cbuildbot_config.GS_PATH_DEFAULT:
    gsutil_archive = 'gs://chromeos-archive/' + bot_id
  else:
    gsutil_archive = buildconfig['gs_path']

  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./archive_build.sh',
         '--build_number', str(buildnumber),
         '--to', '/var/www/archive/' + bot_id,
         '--keep_max', str(keep_max),
         '--board', buildconfig['board'],
         ]

  # If we archive to Google Storage
  if gsutil_archive:
    cmd += ['--gsutil_archive', gsutil_archive,
            '--acl', '/home/chrome-bot/slave_archive_acl',
            '--gsd_gen_index',
            '/b/scripts/gsd_generate_index/gsd_generate_index.py',
            '--gsutil', '/b/scripts/slave/gsutil',
            ]

  # Give the right args to archive_build.
  if buildconfig.get('factory_test_mod', True): cmd.append('--factory_test_mod')
  if not buildconfig['archive_build_debug']: cmd.append('--noarchive_debug')
  if not buildconfig.get('test_mod'): cmd.append('--notest_mod')
  if test_tarball: cmd.extend(['--test_tarball', test_tarball])
  if debug: cmd.append('--debug')
  if buildconfig.get('factory_install_mod', True):
    cmd.append('--factory_install_mod')

  useflags = buildconfig.get('useflags')
  if useflags: cmd.extend(['--useflags', ' '.join(useflags)])

  result = None
  try:
    result = cros_lib.RunCommand(cmd, cwd=cwd, redirect_stdout=True,
                                 redirect_stderr=True,
                                 combine_stdout_stderr=True)
  except:
    if result and result.output:
      Warning(result.output)

    raise

  archive_url = None
  archive_dir = None
  url_re = re.compile('^%s=(.*)$' % _CROS_ARCHIVE_URL)
  dir_re = re.compile('^archive to dir\:(.*)$')
  for line in result.output.splitlines():
    url_match = url_re.match(line)
    if url_match:
      archive_url = url_match.group(1).strip()

    dir_match = dir_re.match(line)
    if dir_match:
      archive_dir = dir_match.group(1).strip()

  # assert archive_url, 'Archive Build Failed to Provide Archive URL'
  assert archive_dir, 'Archive Build Failed to Provide Archive Directory'

  # If we didn't upload to Google Storage, no URL should have been
  # returned. However, we can instead build one based on the HTTP
  # server on the buildbot.
  if not gsutil_archive:
    # '/var/www/archive/build/version' becomes:
    # 'archive/build/version'
    http_offset = archive_dir.index('archive/')
    http_dir = archive_dir[http_offset:]

    # 'http://botname/archive/build/version'
    archive_url = 'http://' + socket.gethostname() + '/' + http_dir

  return archive_url, archive_dir


def UploadSymbols(buildroot, board, official):
  """Upload debug symbols for this build."""
  cmd = ['./upload_symbols',
        '--board=%s' % board,
        '--yes',
        '--verbose']

  if official:
    cmd += ['--official_build']

  cwd = os.path.join(buildroot, 'src', 'scripts')

  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True)


def PushImages(buildroot, board, branch_name, archive_dir):
  """Push the generated image to http://chromeos_images."""
  cmd = ['./pushimage',
        '--board=%s' % board,
        '--branch=%s' % branch_name,
        archive_dir]

  cwd = os.path.join(buildroot, 'src', 'scripts')

  # It may be necessary to set "--nolatest" for some builds in the
  # future.
  cros_lib.RunCommand(cmd, cwd=cwd, enter_chroot=True)
