#!/usr/bin/python
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import multiprocessing
import optparse
import os
import re
import sys
import tempfile
import time

if __name__ == '__main__':
  import constants
  sys.path.append(constants.SOURCE_ROOT)

from chromite.lib import cros_build_lib
from chromite.lib.binpkg import (GrabLocalPackageIndex, GrabRemotePackageIndex,
                                 PackageIndex)
"""
This script is used to upload host prebuilts as well as board BINHOSTS.

If the URL starts with 'gs://', we upload using gsutil to Google Storage.
Otherwise, rsync is used.

After a build is successfully uploaded a file is updated with the proper
BINHOST version as well as the target board. This file is defined in GIT_FILE


To read more about prebuilts/binhost binary packages please refer to:
http://sites/chromeos/for-team-members/engineering/releng/prebuilt-binaries-for-streamlining-the-build-process


Example of uploading prebuilt amd64 host files to Google Storage:
./prebuilt.py -p /b/cbuild/build -s -u gs://chromeos-prebuilt

Example of uploading x86-dogfood binhosts to Google Storage:
./prebuilt.py -b x86-dogfood -p /b/cbuild/build/ -u gs://chromeos-prebuilt  -g

Example of uploading prebuilt amd64 host files using rsync:
./prebuilt.py -p /b/cbuild/build -s -u codf30.jail:/tmp
"""

# as per http://crosbug.com/5855 always filter the below packages
_FILTER_PACKAGES = set()
_RETRIES = 3
_GSUTIL_BIN = '/b/build/third_party/gsutil/gsutil'
_HOST_PACKAGES_PATH = 'chroot/var/lib/portage/pkgs'
_CATEGORIES_PATH = 'chroot/etc/portage/categories'
_HOST_TARGET = 'amd64'
_BOARD_PATH = 'chroot/build/%(board)s'
_BOTO_CONFIG = '/home/chrome-bot/external-boto'
# board/board-target/version/packages/'
_REL_BOARD_PATH = 'board/%(board)s/%(version)s/packages'
# host/host-target/version/packages/'
_REL_HOST_PATH = 'host/%(target)s/%(version)s/packages'
# Private overlays to look at for builds to filter
# relative to build path
_PRIVATE_OVERLAY_DIR = 'src/private-overlays'
_BINHOST_BASE_URL = 'http://commondatastorage.googleapis.com/chromeos-prebuilt'
_PREBUILT_BASE_DIR = 'src/third_party/chromiumos-overlay/chromeos/config/'
# Created in the event of new host targets becoming available
_PREBUILT_MAKE_CONF = {'amd64': os.path.join(_PREBUILT_BASE_DIR,
                                             'make.conf.amd64-host')}
_BINHOST_CONF_DIR = 'src/third_party/chromiumos-overlay/chromeos/binhost'


class FiltersEmpty(Exception):
  """Raised when filters are used but none are found."""
  pass


class UploadFailed(Exception):
  """Raised when one of the files uploaded failed."""
  pass

class UnknownBoardFormat(Exception):
  """Raised when a function finds an unknown board format."""
  pass

class GitPushFailed(Exception):
  """Raised when a git push failed after retry."""
  pass


def UpdateLocalFile(filename, value, key='PORTAGE_BINHOST'):
  """Update the key in file with the value passed.
  File format:
    key="value"
  Note quotes are added automatically

  Args:
    filename: Name of file to modify.
    value: Value to write with the key.
    key: The variable key to update. (Default: PORTAGE_BINHOST)
  """
  if os.path.exists(filename):
    file_fh = open(filename)
  else:
    file_fh = open(filename, 'w+')
  file_lines = []
  found = False
  keyval_str = '%(key)s=%(value)s'
  for line in file_fh:
    # Strip newlines from end of line. We already add newlines below.
    line = line.rstrip("\n")

    if len(line.split('=')) != 2:
      # Skip any line that doesn't fit key=val.
      file_lines.append(line)
      continue

    file_var, file_val = line.split('=')
    if file_var == key:
      found = True
      print 'Updating %s=%s to %s="%s"' % (file_var, file_val, key, value)
      value = '"%s"' % value
      file_lines.append(keyval_str % {'key': key, 'value': value})
    else:
      file_lines.append(keyval_str % {'key': file_var, 'value': file_val})

  if not found:
    file_lines.append(keyval_str % {'key': key, 'value': value})

  file_fh.close()
  # write out new file
  new_file_fh = open(filename, 'w')
  new_file_fh.write('\n'.join(file_lines) + '\n')
  new_file_fh.close()


def RevGitPushWithRetry(retries=5):
  """Repo sync and then push git changes in flight.

    Args:
      retries: The number of times to retry before giving up, default: 5

    Raises:
      GitPushFailed if push was unsuccessful after retries
  """
  for retry in range(1, retries + 1):
    try:
      cros_build_lib.RunCommand('repo sync .', shell=True)
      cros_build_lib.RunCommand('git push', shell=True)
      break
    except cros_build_lib.RunCommandError:
      if retry < retries:
        print 'Error pushing changes trying again (%s/%s)' % (retry, retries)
        time.sleep(5 * retry)
  else:
    raise GitPushFailed('Failed to push change after %s retries' % retries)


def RevGitFile(filename, value, retries=5, key='PORTAGE_BINHOST'):
  """Update and push the git file.

    Args:
      filename: file to modify that is in a git repo already
      value: string representing the version of the prebuilt that has been
        uploaded.
      retries: The number of times to retry before giving up, default: 5
      key: The variable key to update in the git file.
        (Default: PORTAGE_BINHOST)
  """
  prebuilt_branch = 'prebuilt_branch'
  old_cwd = os.getcwd()
  os.chdir(os.path.dirname(filename))

  commit = cros_build_lib.RunCommand('git rev-parse HEAD', shell=True,
                                     redirect_stdout=True).output
  cros_build_lib.RunCommand('git remote update', shell=True)
  cros_build_lib.RunCommand('repo start %s .' % prebuilt_branch, shell=True)
  git_ssh_config_cmd = (
    'git config url.ssh://git@gitrw.chromium.org:9222.pushinsteadof '
    'http://git.chromium.org/git')
  cros_build_lib.RunCommand(git_ssh_config_cmd, shell=True)
  description = 'Update %s="%s" in %s' % (key, value, filename)
  print description
  try:
    UpdateLocalFile(filename, value, key)
    cros_build_lib.RunCommand('git config push.default tracking', shell=True)
    cros_build_lib.RunCommand('git commit -am "%s"' % description, shell=True)
    RevGitPushWithRetry(retries)
  finally:
    cros_build_lib.RunCommand('repo abandon %s .' % prebuilt_branch, shell=True)
    cros_build_lib.RunCommand('git checkout %s' % commit, shell=True)
    os.chdir(old_cwd)


def GetVersion():
  """Get the version to put in LATEST and update the git version with."""
  return datetime.datetime.now().strftime('%d.%m.%y.%H%M%S')


def LoadPrivateFilters(build_path):
  """Load private filters based on ebuilds found under _PRIVATE_OVERLAY_DIR.

  This function adds filters to the global set _FILTER_PACKAGES.
  Args:
    build_path: Path that _PRIVATE_OVERLAY_DIR is in.
  """
  # TODO(scottz): eventually use manifest.xml to find the proper
  # private overlay path.
  filter_path = os.path.join(build_path, _PRIVATE_OVERLAY_DIR)
  files = cros_build_lib.ListFiles(filter_path)
  filters = []
  for file in files:
    if file.endswith('.ebuild'):
      basename = os.path.basename(file)
      match = re.match('(.*?)-\d.*.ebuild', basename)
      if match:
        filters.append(match.group(1))

  if not filters:
    raise FiltersEmpty('No filters were returned')

  _FILTER_PACKAGES.update(filters)


def ShouldFilterPackage(file_path):
  """Skip a particular file if it matches a pattern.

  Skip any files that machine the list of packages to filter in
  _FILTER_PACKAGES.

  Args:
    file_path: string of a file path to inspect against _FILTER_PACKAGES

  Returns:
    True if we should filter the package,
    False otherwise.
  """
  for name in _FILTER_PACKAGES:
    if name in file_path:
      print 'FILTERING %s' % file_path
      return True

  return False


def _RetryRun(cmd, print_cmd=True, shell=False, cwd=None):
  """Run the specified command, retrying if necessary.

  Args:
    cmd: The command to run.
    print_cmd: Whether to print out the cmd.
    shell: Whether to treat the command as a shell.
    cwd: Working directory to run command in.

  Returns:
    True if the command succeeded. Otherwise, returns False.
  """

  # TODO(scottz): port to use _Run or similar when it is available in
  # cros_build_lib.
  for attempt in range(_RETRIES):
    try:
      output = cros_build_lib.RunCommand(cmd, print_cmd=print_cmd, shell=shell,
                                         cwd=cwd)
      return True
    except cros_build_lib.RunCommandError:
      print 'Failed to run %s' % cmd
  else:
    print 'Retry failed run %s, giving up' % cmd
    return False


def _GsUpload(args):
  """Upload to GS bucket.

  Args:
    args: a tuple of three arguments that contains local_file, remote_file, and
          the acl used for uploading the file.

  Returns:
    Return the arg tuple of two if the upload failed
  """
  (local_file, remote_file, acl) = args

  cmd = '%s cp -a %s %s %s' % (_GSUTIL_BIN, acl, local_file, remote_file)
  if not _RetryRun(cmd, print_cmd=False, shell=True):
    return (local_file, remote_file)

def RemoteUpload(acl, files, pool=10):
  """Upload to google storage.

  Create a pool of process and call _GsUpload with the proper arguments.

  Args:
    acl: The canned acl used for uploading. acl can be one of: "public-read",
         "public-read-write", "authenticated-read", "bucket-owner-read",
         "bucket-owner-full-control", or "private".
    files: dictionary with keys to local files and values to remote path.
    pool: integer of maximum proesses to have at the same time.

  Returns:
    Return a set of tuple arguments of the failed uploads
  """
  # TODO(scottz) port this to use _RunManyParallel when it is available in
  # cros_build_lib
  pool = multiprocessing.Pool(processes=pool)
  workers = []
  for local_file, remote_path in files.iteritems():
    workers.append((local_file, remote_path, acl))

  result = pool.map_async(_GsUpload, workers, chunksize=1)
  while True:
    try:
      return set(result.get(60 * 60))
    except multiprocessing.TimeoutError:
      pass


def GenerateUploadDict(base_local_path, base_remote_path, pkgs):
  """Build a dictionary of local remote file key pairs to upload.

  Args:
    base_local_path: The base path to the files on the local hard drive.
    remote_path: The base path to the remote paths.
    pkgs: The packages to upload.

  Returns:
    Returns a dictionary of local_path/remote_path pairs
  """
  upload_files = {}
  for pkg in pkgs:
    suffix = pkg['CPV'] + '.tbz2'
    local_path = os.path.join(base_local_path, suffix)
    assert os.path.exists(local_path)
    remote_path = '%s/%s' % (base_remote_path.rstrip('/'), suffix)
    upload_files[local_path] = remote_path

  return upload_files

def GetBoardPathFromCrosOverlayList(build_path, target):
  """Use the cros_overlay_list to determine the path to the board overlay
   Args:
     build_path: The path to the root of the build directory
     target: The target that we are looking for, could consist of board and
       board_variant, we handle that properly
   Returns:
     The last line from cros_overlay_list as a string
  """
  script_dir = os.path.join(build_path, 'src/platform/dev/host')
  cmd = ['./cros_overlay_list']
  if re.match('.*?_.*', target):
    (board, variant) = target.split('_')
    cmd += ['--board', board, '--variant', variant]
  elif re.match('.*?-\w+', target):
    cmd += ['--board', target]
  else:
    raise UnknownBoardFormat('Unknown format: %s' % target)

  cmd_output = cros_build_lib.RunCommand(cmd, redirect_stdout=True,
                                         cwd=script_dir)
  # We only care about the last entry
  return cmd_output.output.splitlines().pop()


def DeterminePrebuiltConfFile(build_path, target):
  """Determine the prebuilt.conf file that needs to be updated for prebuilts.

    Args:
      build_path: The path to the root of the build directory
      target: String representation of the board. This includes host and board
        targets

    Returns
      A string path to a prebuilt.conf file to be updated.
  """
  if _HOST_TARGET == target:
    # We are host.
    # Without more examples of hosts this is a kludge for now.
    # TODO(Scottz): as new host targets come online expand this to
    # work more like boards.
    make_path = _PREBUILT_MAKE_CONF[target]
  else:
    # We are a board
    board = GetBoardPathFromCrosOverlayList(build_path, target)
    make_path = os.path.join(board, 'prebuilt.conf')

  return make_path


def UpdateBinhostConfFile(path, key, value):
  """Update binhost config file file with key=value.

  Args:
    path: Filename to update.
    key: Key to update.
    value: New value for key.
  """
  cwd = os.path.dirname(os.path.abspath(path))
  filename = os.path.basename(path)
  if not os.path.isdir(cwd):
    os.makedirs(cwd)
  if not os.path.isfile(path):
    config_file = file(path, 'w')
    config_file.close()
  UpdateLocalFile(path, value, key)
  cros_build_lib.RunCommand('git add %s' % filename, cwd=cwd, shell=True)
  description = 'Update %s=%s in %s' % (key, value, filename)
  cros_build_lib.RunCommand('git commit -m "%s"' % description, cwd=cwd,
      shell=True)


def _GrabAllRemotePackageIndexes(binhost_urls):
  """Grab all of the packages files associated with a list of binhost_urls.

  Args:
    binhost_urls: The URLs for the directories containing the Packages files we
                  want to grab.

  Returns:
    A list of PackageIndex objects.
  """
  pkg_indexes = []
  for url in binhost_urls:
    pkg_index = GrabRemotePackageIndex(url)
    if pkg_index:
      pkg_indexes.append(pkg_index)
  return pkg_indexes



class PrebuiltUploader(object):
  """Synchronize host and board prebuilts."""

  def __init__(self, upload_location, acl, binhost_base_url, pkg_indexes):
    """Constructor for prebuilt uploader object.

    This object can upload host or prebuilt files to Google Storage.

    Args:
      upload_location: The upload location.
      acl: The canned acl used for uploading to Google Storage. acl can be one
           of: "public-read", "public-read-write", "authenticated-read",
           "bucket-owner-read", "bucket-owner-full-control", or "private". If
           we are not uploading to Google Storage, this parameter is unused.
      binhost_base_url: The URL used for downloading the prebuilts.
      pkg_indexes: Old uploaded prebuilts to compare against. Instead of
          uploading duplicate files, we just link to the old files.
    """
    self._upload_location = upload_location
    self._acl = acl
    self._binhost_base_url = binhost_base_url
    self._pkg_indexes = pkg_indexes

  def _UploadPrebuilt(self, package_path, url_suffix):
    """Upload host or board prebuilt files to Google Storage space.

    Args:
      package_path: The path to the packages dir.
      url_suffix: The remote subdirectory where we should upload the packages.
    """

    # Process Packages file, removing duplicates and filtered packages.
    pkg_index = GrabLocalPackageIndex(package_path)
    pkg_index.SetUploadLocation(self._binhost_base_url, url_suffix)
    pkg_index.RemoveFilteredPackages(ShouldFilterPackage)
    uploads = pkg_index.ResolveDuplicateUploads(self._pkg_indexes)

    # Write Packages file.
    tmp_packages_file = pkg_index.WriteToNamedTemporaryFile()

    remote_location = '%s/%s' % (self._upload_location.rstrip('/'), url_suffix)
    if remote_location.startswith('gs://'):
      # Build list of files to upload.
      upload_files = GenerateUploadDict(package_path, remote_location, uploads)
      remote_file = '%s/Packages' % remote_location.rstrip('/')
      upload_files[tmp_packages_file.name] = remote_file

      failed_uploads = RemoteUpload(self._acl, upload_files)
      if len(failed_uploads) > 1 or (None not in failed_uploads):
        error_msg = ['%s -> %s\n' % args for args in failed_uploads]
        raise UploadFailed('Error uploading:\n%s' % error_msg)
    else:
      pkgs = ' '.join(p['CPV'] + '.tbz2' for p in uploads)
      ssh_server, remote_path = remote_location.split(':', 1)
      d = { 'pkg_index': tmp_packages_file.name,
            'pkgs': pkgs,
            'remote_packages': '%s/Packages' % remote_location.rstrip('/'),
            'remote_path': remote_path.rstrip('/'),
            'remote_location': remote_location.rstrip('/'),
            'ssh_server': ssh_server }
      cmds = ['ssh %(ssh_server)s mkdir -p %(remote_path)s' % d,
              'rsync -av --chmod=a+r %(pkg_index)s %(remote_packages)s' % d]
      if pkgs:
        cmds.append('rsync -Rav %(pkgs)s %(remote_location)s/' % d)
      for cmd in cmds:
        if not _RetryRun(cmd, shell=True, cwd=package_path):
          raise UploadFailed('Could not run %s' % cmd)

  def _SyncHostPrebuilts(self, build_path, version, key, git_sync,
                        sync_binhost_conf):
    """Synchronize host prebuilt files.

    This function will sync both the standard host packages, plus the host
    packages associated with all targets that have been "setup" with the
    current host's chroot. For instance, if this host has been used to build
    x86-generic, it will sync the host packages associated with
    'i686-pc-linux-gnu'. If this host has also been used to build arm-generic,
    it will also sync the host packages associated with
    'armv7a-cros-linux-gnueabi'.

    Args:
      build_path: The path to the directory containing the chroot.
      version: A unique string, intended to be included in the upload path,
          which identifies the version number of the uploaded prebuilts.
      key: The variable key to update in the git file.
      git_sync: If set, update make.conf of target to reference the latest
          prebuilt packages generated here.
      sync_binhost_conf: If set, update binhost config file in
          chromiumos-overlay for the host.
    """
    # Upload prebuilts.
    package_path = os.path.join(build_path, _HOST_PACKAGES_PATH)
    url_suffix = _REL_HOST_PATH % {'version': version, 'target': _HOST_TARGET}
    self._UploadPrebuilt(package_path, url_suffix)

    # Record URL where prebuilts were uploaded.
    url_value = '%s/%s/' % (self._binhost_base_url.rstrip('/'),
                            url_suffix.rstrip('/'))
    if git_sync:
      git_file = os.path.join(build_path, _PREBUILT_MAKE_CONF[_HOST_TARGET])
      RevGitFile(git_file, url_value, key=key)
    if sync_binhost_conf:
      binhost_conf = os.path.join(build_path, _BINHOST_CONF_DIR, 'host',
          '%s-%s.conf' % (_HOST_TARGET, key))
      UpdateBinhostConfFile(binhost_conf, key, url_value)

  def _SyncBoardPrebuilts(self, board, build_path, version, key, git_sync,
                         sync_binhost_conf):
    """Synchronize board prebuilt files.

    Args:
      board: The board to upload to Google Storage.
      build_path: The path to the directory containing the chroot.
      version: A unique string, intended to be included in the upload path,
          which identifies the version number of the uploaded prebuilts.
      key: The variable key to update in the git file.
      git_sync: If set, update make.conf of target to reference the latest
          prebuilt packages generated here.
      sync_binhost_conf: If set, update binhost config file in
          chromiumos-overlay for the current board.
    """
    # Upload prebuilts.
    board_path = os.path.join(build_path, _BOARD_PATH % {'board': board})
    package_path = os.path.join(board_path, 'packages')
    url_suffix = _REL_BOARD_PATH % {'board': board, 'version': version}
    self._UploadPrebuilt(package_path, url_suffix)

    # Record URL where prebuilts were uploaded.
    url_value = '%s/%s/' % (self._binhost_base_url.rstrip('/'),
                            url_suffix.rstrip('/'))
    if git_sync:
      git_file = DeterminePrebuiltConfFile(build_path, board)
      RevGitFile(git_file, url_value, key=key)
    if sync_binhost_conf:
      binhost_conf = os.path.join(build_path, _BINHOST_CONF_DIR, 'target',
          '%s-%s.conf' % (board, key))
      UpdateBinhostConfFile(binhost_conf, key, url_value)


def usage(parser, msg):
  """Display usage message and parser help then exit with 1."""
  print >> sys.stderr, msg
  parser.print_help()
  sys.exit(1)

def ParseOptions():
  parser = optparse.OptionParser()
  parser.add_option('-H', '--binhost-base-url', dest='binhost_base_url',
                    default=_BINHOST_BASE_URL,
                    help='Base URL to use for binhost in make.conf updates')
  parser.add_option('', '--previous-binhost-url', action='append',
                    default=[], dest='previous_binhost_url',
                    help='Previous binhost URL')
  parser.add_option('-b', '--board', dest='board', default=None,
                    help='Board type that was built on this machine')
  parser.add_option('-p', '--build-path', dest='build_path',
                    help='Path to the directory containing the chroot')
  parser.add_option('-s', '--sync-host', dest='sync_host',
                    default=False, action='store_true',
                    help='Sync host prebuilts')
  parser.add_option('-g', '--git-sync', dest='git_sync',
                    default=False, action='store_true',
                    help='Enable git version sync (This commits to a repo)')
  parser.add_option('-u', '--upload', dest='upload',
                    default=None,
                    help='Upload location')
  parser.add_option('-V', '--prepend-version', dest='prepend_version',
                    default=None,
                    help='Add an identifier to the front of the version')
  parser.add_option('-f', '--filters', dest='filters', action='store_true',
                    default=False,
                    help='Turn on filtering of private ebuild packages')
  parser.add_option('-k', '--key', dest='key',
                    default='PORTAGE_BINHOST',
                    help='Key to update in make.conf / binhost.conf')
  parser.add_option('', '--sync-binhost-conf', dest='sync_binhost_conf',
                    default=False, action='store_true',
                    help='Update binhost.conf')
  parser.add_option('-P', '--private', dest='private', action='store_true',
                    default=False, help='Mark gs:// uploads as private.')

  options, args = parser.parse_args()
  if not options.build_path:
    usage(parser, 'Error: you need provide a chroot path')
  if not options.upload:
    usage(parser, 'Error: you need to provide an upload location using -u')
  if options.private and not (options.binhost_base_url.startswith('gs://') and
                              options.upload.startswith('gs://')):
    usage(parser, 'Error: --private is only valid for gs:// URLs.\n'
                  'Both --binhost-base-url and --upload must be gs:// URLs.')
  return options

def main():
  options = ParseOptions()

  # Setup boto environment for gsutil to use
  os.environ['BOTO_CONFIG'] = _BOTO_CONFIG

  if options.filters:
    LoadPrivateFilters(options.build_path)

  acl = 'public-read'
  if options.private:
    acl = 'private'

  # Calculate a list of Packages index files to compare against. Whenever we
  # upload a package, we check to make sure it's not already stored in one of
  # the packages files we uploaded. This list of packages files might contain
  # both board and host packages.
  pkg_indexes = _GrabAllRemotePackageIndexes(options.previous_binhost_url)

  version = GetVersion()
  if options.prepend_version:
    version = '%s-%s' % (options.prepend_version, version)

  uploader = PrebuiltUploader(options.upload, acl, options.binhost_base_url,
                              pkg_indexes)

  if options.sync_host:
    uploader._SyncHostPrebuilts(options.build_path, version, options.key,
                                options.git_sync, options.sync_binhost_conf)

  if options.board:
    uploader._SyncBoardPrebuilts(options.board, options.build_path, version,
                                 options.key, options.git_sync,
                                 options.sync_binhost_conf)

if __name__ == '__main__':
  main()
