#!/usr/bin/python

# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""CBuildbot is wrapper around the build process used by the pre-flight queue"""

import errno
import heapq
import re
import optparse
import os
import shutil
import sys

if __name__ == '__main__':
  import constants
  sys.path.append(constants.SOURCE_ROOT)

import cbuildbot_comm
from cbuildbot_config import config
from chromite.lib.cros_build_lib import (Die, Info, ReinterpretPathForChroot,
                                         OldRunCommand, Warning, RunCommand)

_DEFAULT_RETRIES = 3
_PACKAGE_FILE = '%(buildroot)s/src/scripts/cbuildbot_package.list'
ARCHIVE_BASE = '/var/www/archive'
ARCHIVE_COUNT = 10
PUBLIC_OVERLAY = '%(buildroot)s/src/third_party/chromiumos-overlay'
PRIVATE_OVERLAY = '%(buildroot)s/src/private-overlays/chromeos-overlay'
CHROME_KEYWORDS_FILE = ('/build/%(board)s/etc/portage/package.keywords/chrome')

_FULL_BINHOST = 'PORTAGE_BINHOST'
_PREFLIGHT_BINHOST = 'PREFLIGHT_BINHOST'
_CHROME_BINHOST = 'CHROME_BINHOST'
_CROS_ARCHIVE_URL = 'CROS_ARCHIVE_URL'

# ======================== Utility functions ================================

def _PrintFile(path):
  """Prints out the contents of a file to stderr."""
  file_handle = open(path)
  print >> sys.stderr, file_handle.read()
  file_handle.close()
  sys.stderr.flush()


def MakeDir(path, parents=False):
  """Basic wrapper around os.mkdirs.

  Keyword arguments:
  path -- Path to create.
  parents -- Follow mkdir -p logic.

  """
  try:
    os.makedirs(path)
  except OSError, e:
    if e.errno == errno.EEXIST and parents:
      pass
    else:
      raise


def RepoSync(buildroot, retries=_DEFAULT_RETRIES):
  """Uses repo to checkout the source code.

  Keyword arguments:
  retries -- Number of retries to try before failing on the sync.
  """
  while retries > 0:
    try:
      # The --trace option ensures that repo shows the output from git. This
      # is needed so that the buildbot can kill us if git is not making
      # progress.
      OldRunCommand(['repo', '--trace', 'sync'], cwd=buildroot)
      OldRunCommand(['repo', 'forall', '-c', 'git', 'config',
                  'url.ssh://git@gitrw.chromium.org:9222.insteadof',
                  'http://git.chromium.org/git'], cwd=buildroot)
      retries = 0
    except:
      retries -= 1
      if retries > 0:
        Warning('CBUILDBOT -- Repo Sync Failed, retrying')
      else:
        Warning('CBUILDBOT -- Retries exhausted')
        raise

  OldRunCommand(['repo', 'manifest', '-r', '-o', '/dev/stderr'], cwd=buildroot)

# =========================== Command Helpers =================================

def _GetAllGitRepos(buildroot, debug=False):
  """Returns a list of tuples containing [git_repo, src_path]."""
  manifest_tuples = []
  # Gets all the git repos from a full repo manifest.
  repo_cmd = "repo manifest -o -".split()
  output = OldRunCommand(repo_cmd, cwd=buildroot, redirect_stdout=True,
                      redirect_stderr=True, print_cmd=debug)

  # Extract all lines containg a project.
  extract_cmd = ['grep', 'project name=']
  output = OldRunCommand(extract_cmd, cwd=buildroot, input=output,
                      redirect_stdout=True, print_cmd=debug)
  # Parse line using re to get tuple.
  result_array = re.findall('.+name=\"([\w-]+)\".+path=\"(\S+)".+', output)

  # Create the array.
  for result in result_array:
    if len(result) != 2:
      Warning('Found incorrect xml object %s' % result)
    else:
      # Remove pre-pended src directory from manifest.
      manifest_tuples.append([result[0], result[1].replace('src/', '')])

  return manifest_tuples


def _GetCrosWorkOnSrcPath(buildroot, board, package, debug=False):
  """Returns ${CROS_WORKON_SRC_PATH} for given package."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  equery_cmd = ('equery-%s which %s' % (board, package)).split()
  ebuild_path = OldRunCommand(equery_cmd, cwd=cwd, redirect_stdout=True,
                             redirect_stderr=True, enter_chroot=True,
                             error_ok=True, print_cmd=debug)
  if ebuild_path:
    ebuild_cmd = ('ebuild-%s %s info' % (board, ebuild_path)).split()
    cros_workon_output = OldRunCommand(ebuild_cmd, cwd=cwd,
                                    redirect_stdout=True, redirect_stderr=True,
                                    enter_chroot=True, print_cmd=debug)

    temp = re.findall('CROS_WORKON_SRCDIR="(\S+)"', cros_workon_output)
    if temp:
      return temp[0]

  return None


def _CreateRepoDictionary(buildroot, board, debug=False):
  """Returns the repo->list_of_ebuilds dictionary."""
  repo_dictionary = {}
  manifest_tuples = _GetAllGitRepos(buildroot)
  Info('Creating dictionary of git repos to portage packages ...')

  cwd = os.path.join(buildroot, 'src', 'scripts')
  get_all_workon_pkgs_cmd = './cros_workon list --all'.split()
  packages = OldRunCommand(get_all_workon_pkgs_cmd, cwd=cwd,
                        redirect_stdout=True, redirect_stderr=True,
                        enter_chroot=True, print_cmd=debug)
  for package in packages.split():
    cros_workon_src_path = _GetCrosWorkOnSrcPath(buildroot, board, package)
    if cros_workon_src_path:
      for tuple in manifest_tuples:
        # This path tends to have the user's home_dir prepended to it.
        if cros_workon_src_path.endswith(tuple[1]):
          Info('For %s found matching package %s' % (tuple[0], package))
          if repo_dictionary.has_key(tuple[0]):
            repo_dictionary[tuple[0]] += [package]
          else:
            repo_dictionary[tuple[0]] = [package]

  return repo_dictionary


def _ParseRevisionString(revision_string, repo_dictionary):
  """Parses the given revision_string into a revision dictionary.

  Returns a list of tuples that contain [portage_package_name, commit_id] to
  update.

  Keyword arguments:
  revision_string -- revision_string with format
      'repo1.git@commit_1 repo2.git@commit2 ...'.
  repo_dictionary -- dictionary with git repository names as keys (w/out git)
      to portage package names.

  """
  # Using a dictionary removes duplicates.
  revisions = {}
  for revision in revision_string.split():
    # Format 'package@commit-id'.
    revision_tuple = revision.split('@')
    if len(revision_tuple) != 2:
      Warning('Incorrectly formatted revision %s' % revision)

    repo_name = revision_tuple[0].replace('.git', '')
    # Might not have entry if no matching ebuild.
    if repo_dictionary.has_key(repo_name):
      # May be many corresponding packages to a given git repo e.g. kernel).
      for package in repo_dictionary[repo_name]:
        revisions[package] = revision_tuple[1]

  return revisions.items()


def _UprevFromRevisionList(buildroot, tracking_branch, revision_list, board,
                           overlays):
  """Uprevs based on revision list."""
  if not revision_list:
    Info('No packages found to uprev')
    return

  packages = []
  for package, revision in revision_list:
    assert ':' not in package, 'Invalid package name: %s' % package
    packages.append(package)

  chroot_overlays = [ReinterpretPathForChroot(path) for path in overlays]

  cwd = os.path.join(buildroot, 'src', 'scripts')
  OldRunCommand(['../../chromite/buildbot/cros_mark_as_stable',
              '--board=%s' % board,
              '--tracking_branch=%s' % tracking_branch,
              '--overlays=%s' % ':'.join(chroot_overlays),
              '--packages=%s' % ':'.join(packages),
              '--drop_file=%s' % ReinterpretPathForChroot(_PACKAGE_FILE %
                  {'buildroot': buildroot}),
              'commit'],
             cwd=cwd, enter_chroot=True)


def _MarkChromeAsStable(buildroot, tracking_branch, chrome_rev, board):
  """Returns the portage atom for the revved chrome ebuild - see man emerge."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  portage_atom_string = OldRunCommand(
      ['../../chromite/buildbot/cros_mark_chrome_as_stable',
       '--tracking_branch=%s' % tracking_branch,
       chrome_rev],
      cwd=cwd, redirect_stdout=True, enter_chroot=True).rstrip()
  if not portage_atom_string:
    Info('Found nothing to rev.')
    return None
  else:
    chrome_atom = portage_atom_string.split('=')[1]
    keywords_file = CHROME_KEYWORDS_FILE % {'board': board}
    # TODO(sosa): Workaround to build unstable chrome ebuild we uprevved.
    OldRunCommand(['sudo', 'mkdir', '-p', os.path.dirname(keywords_file)],
               enter_chroot=True, cwd=cwd)
    OldRunCommand(['sudo', 'tee', keywords_file], input='=%s\n' % chrome_atom,
               enter_chroot=True, cwd=cwd)
    return chrome_atom


def _UprevAllPackages(buildroot, tracking_branch, board, overlays):
  """Uprevs all packages that have been updated since last uprev."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  chroot_overlays = [ReinterpretPathForChroot(path) for path in overlays]
  OldRunCommand(['../../chromite/buildbot/cros_mark_as_stable', '--all',
              '--board=%s' % board,
              '--overlays=%s' % ':'.join(chroot_overlays),
              '--tracking_branch=%s' % tracking_branch,
              '--drop_file=%s' % ReinterpretPathForChroot(_PACKAGE_FILE %
                  {'buildroot': buildroot}),
              'commit'],
              cwd=cwd, enter_chroot=True)


def _GetVMConstants(buildroot):
  """Returns minimum (vdisk_size, statefulfs_size) recommended for VM's."""
  cwd = os.path.join(buildroot, 'src', 'scripts', 'lib')
  source_cmd = 'source %s/cros_vm_constants.sh' % cwd
  vdisk_size = OldRunCommand([
      '/bin/bash', '-c', '%s && echo $MIN_VDISK_SIZE_FULL' % source_cmd],
       redirect_stdout=True)
  statefulfs_size = OldRunCommand([
      '/bin/bash', '-c', '%s && echo $MIN_STATEFUL_FS_SIZE_FULL' % source_cmd],
       redirect_stdout=True)
  return (vdisk_size.strip(), statefulfs_size.strip())


def _GitCleanup(buildroot, board, tracking_branch, overlays):
  """Clean up git branch after previous uprev attempt."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  if os.path.exists(cwd):
    OldRunCommand(
        ['../../chromite/buildbot/cros_mark_as_stable', '--srcroot=..',
         '--board=%s' % board,
         '--overlays=%s' % ':'.join(overlays),
         '--tracking_branch=%s' % tracking_branch, 'clean'],
        cwd=cwd, error_ok=True)


def _CleanUpMountPoints(buildroot):
  """Cleans up any stale mount points from previous runs."""
  mount_output = OldRunCommand(['mount'], redirect_stdout=True)
  mount_pts_in_buildroot = OldRunCommand(['grep', buildroot],
                                         input=mount_output,
                                         redirect_stdout=True, error_ok=True)

  for mount_pt_str in mount_pts_in_buildroot.splitlines():
    mount_pt = mount_pt_str.rpartition(' type ')[0].partition(' on ')[2]
    OldRunCommand(['sudo', 'umount', '-l', mount_pt], error_ok=True)


def _WipeOldOutput(buildroot):
  """Wipes out build output directories."""
  OldRunCommand(['rm', '-rf', 'src/build/images'], cwd=buildroot)


# =========================== Main Commands ===================================


def _PreFlightRinse(buildroot, board, tracking_branch, overlays):
  """Cleans up any leftover state from previous runs."""
  _GitCleanup(buildroot, board, tracking_branch, overlays)
  _CleanUpMountPoints(buildroot)
  OldRunCommand(['sudo', 'killall', 'kvm'], error_ok=True)


def _FullCheckout(buildroot, tracking_branch,
                  retries=_DEFAULT_RETRIES,
                  url='http://git.chromium.org/git/manifest'):
  """Performs a full checkout and clobbers any previous checkouts."""
  OldRunCommand(['sudo', 'rm', '-rf', buildroot])
  MakeDir(buildroot, parents=True)
  branch = tracking_branch.split('/');
  OldRunCommand(['repo', 'init', '-u',
             url, '-b',
             '%s' % branch[-1]], cwd=buildroot, input='\n\ny\n')
  RepoSync(buildroot, retries)


def _IncrementalCheckout(buildroot, retries=_DEFAULT_RETRIES):
  """Performs a checkout without clobbering previous checkout."""
  RepoSync(buildroot, retries)


def _MakeChroot(buildroot, replace=False):
  """Wrapper around make_chroot."""
  cwd = os.path.join(buildroot, 'src', 'scripts')

  cmd = ['./make_chroot', '--fast']

  if replace:
    cmd.append('--replace')

  OldRunCommand(cmd, cwd=cwd)


def _GetPortageEnvVar(buildroot, board, envvar):
  """Get a portage environment variable for the specified board, if any.

  buildroot: The root directory where the build occurs. Must be an absolute
             path.
  board: Board type that was built on this machine. E.g. x86-generic. If this
         is None, get the env var from the host.
  envvar: The environment variable to get. E.g. 'PORTAGE_BINHOST'.

  Returns:
    The value of the environment variable, as a string. If no such variable
    can be found, return the empty string.
  """
  cwd = os.path.join(buildroot, 'src', 'scripts')
  portageq = 'portageq'
  if board:
    portageq += '-%s' % board
  binhost = OldRunCommand([portageq, 'envvar', envvar], cwd=cwd,
                       redirect_stdout=True, enter_chroot=True, error_ok=True)
  return binhost.rstrip('\n')


def _SetupBoard(buildroot, board='x86-generic'):
  """Wrapper around setup_board."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  OldRunCommand(['./setup_board', '--fast', '--default', '--board=%s' % board],
             cwd=cwd, enter_chroot=True)


def _Build(buildroot, emptytree, build_autotest=True, usepkg=True):
  """Wrapper around build_packages."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  if emptytree:
    cmd = ['sh', '-c', 'EXTRA_BOARD_FLAGS=--emptytree ./build_packages']
  else:
    cmd = ['./build_packages']

  if not build_autotest:
    cmd.append('--nowithautotest')

  if not usepkg:
    cmd.append('--nousepkg')

  OldRunCommand(cmd, cwd=cwd, enter_chroot=True)


def _EnableLocalAccount(buildroot):
  cwd = os.path.join(buildroot, 'src', 'scripts')
  # Set local account for test images.
  OldRunCommand(['./enable_localaccount.sh',
             'chronos'],
             print_cmd=False, cwd=cwd)


def _BuildImage(buildroot):
  _WipeOldOutput(buildroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  OldRunCommand(['./build_image', '--replace'], cwd=cwd, enter_chroot=True)


def _BuildVMImageForTesting(buildroot):
  (vdisk_size, statefulfs_size) = _GetVMConstants(buildroot)
  cwd = os.path.join(buildroot, 'src', 'scripts')
  OldRunCommand(['./image_to_vm.sh',
              '--test_image',
              '--full',
              '--vdisk_size=%s' % vdisk_size,
              '--statefulfs_size=%s' % statefulfs_size,
              ], cwd=cwd, enter_chroot=True)


def _RunUnitTests(buildroot):
  cwd = os.path.join(buildroot, 'src', 'scripts')
  OldRunCommand(['./cros_run_unit_tests',
              '--package_file=%s' % ReinterpretPathForChroot(_PACKAGE_FILE %
                  {'buildroot': buildroot}),
             ], cwd=cwd, enter_chroot=True)


def _RunSmokeSuite(buildroot, results_dir):
  results_dir_in_chroot = os.path.join(buildroot, 'chroot',
                                       results_dir.lstrip('/'))
  if os.path.exists(results_dir_in_chroot):
    shutil.rmtree(results_dir_in_chroot)

  cwd = os.path.join(buildroot, 'src', 'scripts')
  OldRunCommand(['bin/cros_run_vm_test',
              '--no_graphics',
              '--results_dir_root=%s' % results_dir,
              'suite_Smoke',
              ], cwd=cwd, error_ok=False)


def _RunAUTest(buildroot, board):
  """Runs a basic update test from the au test harness."""
  cwd = os.path.join(buildroot, 'src', 'scripts')
  image_path = os.path.join(buildroot, 'src', 'build', 'images', board,
                            'latest', 'chromiumos_test_image.bin')
  OldRunCommand(['bin/cros_au_test_harness',
              '--no_graphics',
              '--no_delta',
              '--board=%s' % board,
              '--test_prefix=SimpleTest',
              '--verbose',
              '--base_image=%s' % image_path,
              '--target_image=%s' % image_path,
              ], cwd=cwd, error_ok=False)


def _UprevPackages(buildroot, tracking_branch, revisionfile, board, overlays):
  """Uprevs a package based on given revisionfile.

  If revisionfile is set to None or does not resolve to an actual file, this
  function will uprev all packages.

  Keyword arguments:
  revisionfile -- string specifying a file that contains a list of revisions to
      uprev.
  """
  # Purposefully set to None as it means Force Build was pressed.
  revisions = 'None'
  if (revisionfile):
    try:
      rev_file = open(revisionfile)
      revisions = rev_file.read()
      rev_file.close()
    except Exception, e:
      Warning('Error reading %s, revving all' % revisionfile)
      revisions = 'None'

  revisions = revisions.strip()

  # TODO(sosa): Un-comment once we close individual trees.
  # revisions == "None" indicates a Force Build.
  #if revisions != 'None':
  #  print >> sys.stderr, 'CBUILDBOT Revision list found %s' % revisions
  #  revision_list = _ParseRevisionString(revisions,
  #      _CreateRepoDictionary(buildroot, board))
  #  _UprevFromRevisionList(buildroot, tracking_branch, revision_list, board,
  #                         overlays)
  #else:
  Info('CBUILDBOT Revving all')
  _UprevAllPackages(buildroot, tracking_branch, board, overlays)


def _UprevPush(buildroot, tracking_branch, board, overlays, dryrun):
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
  OldRunCommand(cmd, cwd=cwd)


def _LegacyArchiveBuild(buildroot, bot_id, buildconfig, buildnumber,
                        test_tarball, debug=False):
  """Archives build artifacts and returns URL to archived location."""

  # Fixed properties
  keep_max = 3
  gsutil_archive = 'gs://chromeos-archive/' + bot_id
  cwd = os.path.join(buildroot, 'src', 'scripts')
  cmd = ['./archive_build.sh',
         '--build_number', str(buildnumber),
         '--to', '/var/www/archive/' + bot_id,
         '--keep_max', str(keep_max),
         '--board', buildconfig['board'],
         '--acl', '/home/chrome-bot/slave_archive_acl',
         '--gsutil_archive', gsutil_archive,
         '--gsd_gen_index',
           '/b/scripts/gsd_generate_index/gsd_generate_index.py',
         '--gsutil', '/b/scripts/slave/gsutil',
  ]
  # Give the right args to archive_build.
  if buildconfig['archive_build_prebuilts']: cmd.append('--prebuilt_upload')
  if buildconfig.get('factory_test_mod', True): cmd.append('--factory_test_mod')
  if not buildconfig['archive_build_debug']: cmd.append('--noarchive_debug')
  if not buildconfig.get('test_mod'): cmd.append('--notest_mod')
  if test_tarball: cmd.extend(['--test_tarball', test_tarball])
  if debug: cmd.append('--debug')
  if buildconfig.get('factory_install_mod', True):
    cmd.append('--factory_install_mod')

  try:
    result = RunCommand(cmd, cwd=cwd, redirect_stdout=True,
                        redirect_stderr=True, combine_stdout_stderr=True)
  except:
    Warning(result.stdout)
    raise

  archive_url = None
  key_re = re.compile('^%s=(.*)$' % _CROS_ARCHIVE_URL)
  for line in result.output.splitlines():
    line_match = key_re.match(line)
    if line_match:
      archive_url = line_match.group(1)

  assert archive_url, 'Archive Build Failed to Provide Archive URL'
  return archive_url


def _ArchiveTestResults(buildroot, test_results_dir):
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
    OldRunCommand(['sudo', 'chmod', '-R', 'a+rw', results_path])

    archive_tarball = os.path.join(buildroot, 'test_results.tgz')
    if os.path.exists(archive_tarball): os.remove(archive_tarball)
    OldRunCommand(['tar', 'czf', archive_tarball, results_path])
    shutil.rmtree(results_path)
    return archive_tarball
  except Exception, e:
    Warning('=================================================================')
    Warning('------>       We failed to archive test results. <---------------')
    Warning(str(e))
    Warning('=================================================================')


def _GetConfig(config_name):
  """Gets the configuration for the build"""
  buildconfig = {}
  if not config.has_key(config_name):
    Warning('Non-existent configuration specified.')
    Warning('Please specify one of:')
    config_names = config.keys()
    config_names.sort()
    for name in config_names:
      Warning('  %s' % name)
    sys.exit(1)

  return config[config_name]


def _ResolveOverlays(buildroot, overlays):
  """Return the list of overlays to use for a given buildbot.

  Args:
    buildroot: The root directory where the build occurs. Must be an absolute
               path.
    overlays: A string describing which overlays you want.
              'private': Just the private overlay.
              'public': Just the public overlay.
              'both': Both the public and private overlays.
  """
  public_overlay = PUBLIC_OVERLAY % {'buildroot': buildroot}
  private_overlay = PRIVATE_OVERLAY % {'buildroot': buildroot}
  if overlays == 'private':
    paths = [private_overlay]
  elif overlays == 'public':
    paths = [public_overlay]
  elif overlays == 'both':
    paths = [public_overlay, private_overlay]
  else:
    Info('No overlays found.')
    paths = []
  return paths


def _UploadPrebuilts(buildroot, board, overlay_config, binhosts, chrome_rev):
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
    chrome_rev: Chrome_rev of type [tot|latest_release|sticky_release].
  """
  cwd = os.path.dirname(__file__)
  cmd = ['./prebuilt.py',
         '--sync-binhost-conf',
         '--build-path', buildroot,
         '--board', board]
  for binhost in binhosts:
    if binhost:
      cmd.extend(['--previous-binhost-url', binhost])
  if overlay_config == 'public':
    cmd.extend(['--upload', 'gs://chromeos-prebuilt'])
  else:
    assert overlay_config in ('private', 'both')
    cmd.extend(['--upload', 'chromeos-images:/var/www/prebuilt/',
                '--binhost-base-url', 'http://chromeos-prebuilt'])
  if chrome_rev:
    key = '%s_%s' % (chrome_rev, _CHROME_BINHOST)
    cmd.extend(['--prepend-version', 'chrome',
                '--key', key.upper()])
  else:
    cmd.extend(['--prepend-version', 'preflight',
                '--key', _PREFLIGHT_BINHOST])

  OldRunCommand(cmd, cwd=cwd)


def main():
  # Parse options
  usage = "usage: %prog [options] cbuildbot_config"
  parser = optparse.OptionParser(usage=usage)
  parser.add_option('-a', '--acl', default='private',
                    help='ACL to set on GSD archives')
  parser.add_option('-r', '--buildroot',
                    help='root directory where build occurs', default=".")
  parser.add_option('-n', '--buildnumber',
                    help='build number', type='int', default=0)
  parser.add_option('--chrome_rev', default=None, type='string',
                    dest='chrome_rev',
                    help=('Chrome_rev of type [tot|latest_release|'
                          'sticky_release]'))
  parser.add_option('-g', '--gsutil', default='', help='Location of gsutil')
  parser.add_option('-c', '--gsutil_archive', default='',
                    help='Datastore archive location')
  parser.add_option('--clobber', action='store_true', dest='clobber',
                    default=False,
                    help='Clobbers an old checkout before syncing')
  parser.add_option('--debug', action='store_true', dest='debug',
                    default=False,
                    help='Override some options to run as a developer.')
  parser.add_option('--nobuild', action='store_false', dest='build',
                    default=True,
                    help="Don't actually build (for cbuildbot dev")
  parser.add_option('--noprebuilts', action='store_false', dest='prebuilts',
                    default=True,
                    help="Don't upload prebuilts.")
  parser.add_option('--nosync', action='store_false', dest='sync',
                    default=True,
                    help="Don't sync before building.")
  parser.add_option('--notests', action='store_false', dest='tests',
                    default=True,
                    help='Override values from buildconfig and run no tests.')
  parser.add_option('-f', '--revisionfile',
                    help='file where new revisions are stored')
  parser.add_option('-t', '--tracking-branch', dest='tracking_branch',
                    default='cros/master', help='Run the buildbot on a branch')
  parser.add_option('-u', '--url', dest='url',
                    default='http://git.chromium.org/git/manifest',
                    help='Run the buildbot on internal manifest')

  (options, args) = parser.parse_args()

  buildroot = os.path.abspath(options.buildroot)
  revisionfile = options.revisionfile
  tracking_branch = options.tracking_branch
  chrome_atom_to_build = None
  archive_url = None

  if len(args) >= 1:
    bot_id = args[-1]
    buildconfig = _GetConfig(bot_id)
  else:
    Warning('Missing configuration description')
    parser.print_usage()
    sys.exit(1)

  try:
    # Calculate list of overlay directories.
    rev_overlays = _ResolveOverlays(buildroot, buildconfig['rev_overlays'])
    push_overlays = _ResolveOverlays(buildroot, buildconfig['push_overlays'])
    # We cannot push to overlays that we don't rev.
    assert set(push_overlays).issubset(set(rev_overlays))
    # Either has to be a master or not have any push overlays.
    assert buildconfig['master'] or not push_overlays

    board = buildconfig['board']
    old_binhost = None

    _PreFlightRinse(buildroot, buildconfig['board'], tracking_branch,
                    rev_overlays)
    chroot_path = os.path.join(buildroot, 'chroot')
    boardpath = os.path.join(chroot_path, 'build', board)
    if options.sync:
      if options.clobber or not os.path.isdir(buildroot):
        _FullCheckout(buildroot, tracking_branch, url=options.url)
      else:
        old_binhost = _GetPortageEnvVar(buildroot, board, _FULL_BINHOST)
        _IncrementalCheckout(buildroot)

    new_binhost = _GetPortageEnvVar(buildroot, board, _FULL_BINHOST)
    emptytree = (old_binhost and old_binhost != new_binhost)

    # Check that all overlays can be found.
    for path in rev_overlays:
      if not os.path.isdir(path):
        Die('Missing overlay: %s' % path)

    if not os.path.isdir(chroot_path) or buildconfig['chroot_replace']:
      _MakeChroot(buildroot, buildconfig['chroot_replace'])

    if not os.path.isdir(boardpath):
      _SetupBoard(buildroot, board=buildconfig['board'])

    # Perform chrome uprev.
    if options.chrome_rev:
      chrome_atom_to_build = _MarkChromeAsStable(buildroot, tracking_branch,
                                                 options.chrome_rev, board)
    # Perform other uprevs.
    if buildconfig['uprev']:
      _UprevPackages(buildroot, tracking_branch, revisionfile,
                     buildconfig['board'], rev_overlays)
    elif options.chrome_rev and not chrome_atom_to_build:
      # We found nothing to rev, we're done here.
      return

    _EnableLocalAccount(buildroot)

    if options.build:
      _Build(buildroot,
             emptytree,
             build_autotest=(buildconfig['vm_tests'] and options.tests),
             usepkg=buildconfig['usepkg'])

      if buildconfig['unittests'] and options.tests:
        _RunUnitTests(buildroot)

      _BuildImage(buildroot)

    if buildconfig['vm_tests'] and options.tests:
      _BuildVMImageForTesting(buildroot)
      test_results_dir = '/tmp/run_remote_tests.%s' % options.buildnumber
      try:
        _RunSmokeSuite(buildroot, test_results_dir)
        _RunAUTest(buildroot, buildconfig['board'])
      finally:
        test_tarball = _ArchiveTestResults(buildroot,
                                           test_results_dir=test_results_dir)
        archive_url = _LegacyArchiveBuild(buildroot, bot_id, buildconfig,
                                          options.buildnumber, test_tarball,
                                          options.debug)

    # Don't push changes for developers.
    if buildconfig['master']:
      # Master bot needs to check if the other slaves completed.
      if cbuildbot_comm.HaveSlavesCompleted(config):
        if not options.debug and options.prebuilts:
          _UploadPrebuilts(buildroot, board, buildconfig['rev_overlays'],
                           [new_binhost], options.chrome_rev)
        _UprevPush(buildroot, tracking_branch, buildconfig['board'],
                   push_overlays, options.debug)
      else:
        Die('CBUILDBOT - One of the slaves has failed!!!')

    else:
      # Publish my status to the master if its expecting it.
      if buildconfig['important'] and not options.debug:
        cbuildbot_comm.PublishStatus(cbuildbot_comm.STATUS_BUILD_COMPLETE)

  except:
    # Send failure to master bot.
    if not buildconfig['master'] and buildconfig['important']:
      cbuildbot_comm.PublishStatus(cbuildbot_comm.STATUS_BUILD_FAILED)

    raise

  finally:
    if archive_url:
      Info('BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:')
      Info(archive_url)


if __name__ == '__main__':
    main()
