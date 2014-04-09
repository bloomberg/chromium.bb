#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download all Native Client toolchains for this platform.

This module downloads multiple tgz's and expands them.
"""

import cygtar
import optparse
import os
import re
import sys
import tempfile

sys.path.append(os.path.join(os.path.dirname(__file__), '..'))
import pynacl.download_utils
import pynacl.file_tools
import pynacl.platform

import toolchainbinaries


SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)

SHA1_IN_FILENAME = re.compile('.*_[0-9a-f]{40}$')


def LoadVersions(filepath):
  """Load version data from specified filepath into a dictionary.

  Arguments:
    filepath: path to the file which will be loaded.
  Returns:
    A dictionary of KEY:VALUE pairs.
  """
  versions = {}
  version_lines = open(filepath, 'r').readlines()
  for line_num, line in enumerate(version_lines, start=1):
    line = line.strip()
    if line.startswith('#'):
      continue

    if line == '':
      continue

    if '=' not in line:
      raise RuntimeError('Expecting KEY=VALUE in line %d:\n\t>>%s<<' %
                         (line_num, line))

    key, val = line.split('=', 1)
    versions[key] = val
  return versions


def VersionSelect(versions, flavor):
  """Determine svn revision based on version data and flavor.

  Arguments:
    versions: version data loaded from file.
    flavor: kind of tool.
  Returns:
    An svn version number (or other version string).
  """

  if isinstance(flavor, tuple):
    ids = [versions[i] for i in flavor[1:]]
    return ','.join(ids)
  if toolchainbinaries.IsBionicFlavor(flavor):
    return versions['BIONIC_VERSION']
  if toolchainbinaries.IsPnaclFlavor(flavor):
    return versions['PNACL_VERSION']
  if toolchainbinaries.IsX86Flavor(flavor):
    if toolchainbinaries.IsNotNaClNewlibFlavor(flavor):
      return versions['GLIBC_VERSION']
    else:
      return versions['NEWLIB_VERSION']
  if toolchainbinaries.IsArmTrustedFlavor(flavor):
    return versions['ARM_TRUSTED_VERSION']
  raise Exception('Unknown flavor "%s"' % flavor)


def HashKey(flavor):
  """Generate the name of the key for this flavor's hash.

  Arguments:
    flavor: kind of tool.
  Returns:
    The string key for the hash.
  """
  return 'NACL_TOOL_%s_HASH' % flavor.upper()


def HashSelect(versions, flavor):
  """Determine expected hash value based on version data and flavor.

  Arguments:
    versions: version data loaded from file.
    flavor: kind of tool.
  Returns:
    A SHA1 hash.
  """
  return versions[HashKey(flavor)]


def IsFlavorNeeded(options, flavor):
  if isinstance(flavor, tuple):
    flavor = flavor[0]
  if options.filter_out_predicates:
    for predicate in options.filter_out_predicates:
      if predicate(flavor):
        return False
  return True


def FlavorOutDir(options, flavor):
  """Given a flavor, decide where it should be extracted."""
  if isinstance(flavor, tuple):
    return toolchainbinaries.GetStandardToolchainFlavorDir(
        options.toolchain_dir,
        flavor[0])
  else:
    return toolchainbinaries.GetStandardToolchainFlavorDir(
        options.toolchain_dir,
        flavor)


def RemoveOldToolchainDir(toolchain_dir):
  """Temporary code to delete old toolchain directory."""
  # Delete any top level directories that do not conform to $OS_$ARCH.
  valid_top_level_dirs = set()
  for os_name in pynacl.platform.OS_LIST:
    for arch_name in pynacl.platform.ARCH_LIST:
      valid_top_level_dirs.add('%s_%s' % (os_name, arch_name))

  # Delete any files and directories that do not conform to the standard.
  # Do not touch any system files that begin with '.', including ".tars".
  top_level_dirs = []
  for dir_item in os.listdir(toolchain_dir):
    if dir_item.startswith('.'):
      continue

    full_path = os.path.join(toolchain_dir, dir_item)
    if dir_item in valid_top_level_dirs:
      top_level_dirs.append(full_path)
      continue
    print 'Removing stale toolchain item:', full_path
    if os.path.isfile(full_path):
      os.unlink(full_path)
    else:
      pynacl.file_tools.RemoveDir(full_path)

  # Delete any second level dirs that do not conform one of the following:
  #  1. It must be of the format "nacl_*".
  #  2. It must be of the format "pnacl_*".
  #  3. It must be of the format "*_trusted".
  for top_level_dir in top_level_dirs:
    for dir_item in os.listdir(top_level_dir):
      if (dir_item.startswith('nacl_') or
          dir_item.startswith('pnacl_') or
          dir_item.endswith('_trusted')):
        continue

      full_path = os.path.join(top_level_dir, dir_item)
      print 'Removing stale toolchain item:', full_path
      if os.path.isfile(full_path):
        os.unlink(full_path)
      else:
        pynacl.file_tools.RemoveDir(full_path)


def FlavorName(flavor):
  """Given a flavor, get a string name for it."""
  if isinstance(flavor, tuple):
    return flavor[0]
  else:
    return flavor


def FlavorComponentNames(flavor):
  if isinstance(flavor, tuple):
    return flavor[1:]
  else:
    return [flavor]


def FlavorUrls(options, versions, flavor):
  """Given a flavor, get a list of the URLs of its components."""
  if isinstance(flavor, tuple):
    ids = [versions[i] for i in flavor[1:]]
    return [toolchainbinaries.EncodeToolchainUrl(
            options.base_once_url, i, 'new') for i in ids]
  else:
    return [toolchainbinaries.EncodeToolchainUrl(
            options.base_url, VersionSelect(versions, flavor), flavor)]


def FlavorHashes(versions, flavor):
  """Given a flavor, get the list of hashes of its components."""
  if isinstance(flavor, tuple):
    return [HashSelect(versions, i) for i in flavor[1:]]
  else:
    return [HashSelect(versions, flavor)]


def GetUpdatedDEPS(options, versions):
  """Return a suggested DEPS toolchain hash update for all platforms.

  Arguments:
    options: options from the command line.
  """
  flavors = set()
  for pm in toolchainbinaries.PLATFORM_MAPPING.itervalues():
    for flavorlist in pm.itervalues():
      for flavor in flavorlist:
        if IsFlavorNeeded(options, flavor):
          flavors.add(flavor)
  new_deps = {}
  for flavor in flavors:
    names = FlavorComponentNames(flavor)
    urls = FlavorUrls(options, versions, flavor)
    for name, url in zip(names, urls):
      new_deps[name] = pynacl.download_utils.HashUrl(url)
  return new_deps


def ShowUpdatedDEPS(options, versions):
  """Print a suggested DEPS toolchain hash update for all platforms.

  Arguments:
    options: options from the command line.
  """
  for flavor, value in sorted(GetUpdatedDEPS(options, versions).iteritems()):
    keyname = HashKey(flavor)
    print '%s=%s' % (keyname, value)
    sys.stdout.flush()


def SyncFlavor(flavor, urls, dst, hashes, min_time, keep=False, force=False,
               verbose=False):
  """Sync a flavor of the nacl toolchain

  Arguments:
    flavor: short directory name of the toolchain flavor.
    urls: urls to download the toolchain flavor from.
    dst: destination directory for the toolchain.
    hashes: expected hashes of the toolchain.
  """
  if isinstance(flavor, tuple):
    flavor_name = flavor[0]
  else:
    flavor_name = flavor

  toolchain_dir = os.path.join(PARENT_DIR, 'toolchain')
  if not os.path.exists(toolchain_dir):
    os.makedirs(toolchain_dir)
  download_dir = os.path.join(toolchain_dir, '.tars')

  prefix = 'tmp_unpacked_toolchain_'
  suffix = '.tmp'

  # Attempt to cleanup.
  for path in os.listdir(toolchain_dir):
    if path.startswith(prefix) and path.endswith(suffix):
      full_path = os.path.join(toolchain_dir, path)
      try:
        print 'Cleaning up %s...' % full_path
        pynacl.file_tools.RemoveDir(full_path)
      except Exception, e:
        print 'Failed cleanup with: ' + str(e)

  # If we are forcing a sync, then ignore stamp
  if force:
    stamp_dir = None
  else:
    stamp_dir = dst

  filepaths = []
  need_sync = False

  index = 0
  for url, hash_val in zip(urls, hashes):
    # Build the tarfile name from the url
    # http://foo..../bar.tar.gz -> bar
    filepath, ext = url.split('/')[-1].split('.', 1)
    # For filenames containing _SHA1s, drop the sha1 part.
    if SHA1_IN_FILENAME.match(filepath) is not None:
      filepath = filepath.rsplit('_', 1)[0]
    # Put it in the download dir and add back extension.
    filepath = os.path.join(download_dir, '.'.join([filepath, ext]))
    filepaths.append(filepath)
    # If we did not need to synchronize, then we are done
    if pynacl.download_utils.SyncURL(url, filepath, stamp_dir=stamp_dir,
                                     min_time=min_time, hash_val=hash_val,
                                     stamp_index=index,
                                     keep=keep, verbose=verbose):
      need_sync = True
    index += 1

  if not need_sync:
    return False

  # Compute the new hashes for each file.
  new_hashes = []
  for filepath in filepaths:
    new_hashes.append(pynacl.download_utils.HashFile(filepath))

  untar_dir = tempfile.mkdtemp(
      suffix=suffix, prefix=prefix, dir=toolchain_dir)
  try:
    if verbose:
      tar_file = os.path.basename(filepath)
      rel_dest = os.path.relpath(dst, toolchain_dir)
      print '%s: Extracting "%s" -> "%s"...' % (flavor_name, tar_file, rel_dest)

    for filepath in filepaths:
      tar = cygtar.CygTar(filepath, 'r:*', verbose=verbose)
      curdir = os.getcwd()
      os.chdir(untar_dir)
      try:
        tar.Extract()
        tar.Close()
      finally:
        os.chdir(curdir)

      if not keep:
        os.remove(filepath)

    # TODO(bradnelson_): get rid of this when toolchain tarballs flattened.
    if 'bionic' in flavor:
      src = os.path.join(untar_dir, flavor)
    elif isinstance(flavor, tuple) or 'arm' in flavor or 'pnacl' in flavor:
      src = os.path.join(untar_dir)
    elif 'newlib' in flavor:
      src = os.path.join(untar_dir, 'sdk', 'nacl-sdk')
    else:
      src = os.path.join(untar_dir, 'toolchain', flavor)
    pynacl.file_tools.MoveDirCleanly(src, dst)
  finally:
    try:
      pynacl.file_tools.RemoveDir(untar_dir)
    except Exception, e:
      print 'Failed cleanup with: ' + str(e)
      print 'Continuing on original exception...'
  pynacl.download_utils.WriteSourceStamp(dst, '\n'.join(urls))
  pynacl.download_utils.WriteHashStamp(dst, '\n'.join(new_hashes))
  return True


def ParseArgs(args):
  parser = optparse.OptionParser()
  parser.add_option(
      '-b', '--base-url', dest='base_url',
      default=toolchainbinaries.BASE_DOWNLOAD_URL,
      help='base url to download from')
  parser.add_option(
      '--base-once-url', dest='base_once_url',
      default=toolchainbinaries.BASE_ONCE_DOWNLOAD_URL,
      help='base url to download new toolchain artifacts from')
  parser.add_option(
      '-c', '--hashes', dest='hashes',
      default=False,
      action='store_true',
      help='Calculate hashes.')
  parser.add_option(
      '-k', '--keep', dest='keep',
      default=False,
      action='store_true',
      help='Keep the downloaded tarballs.')
  parser.add_option(
      '-q', '--quiet', dest='verbose',
      default=True,
      action='store_false',
      help='Produce less output.')
  parser.add_option(
      '--toolchain-dir', dest='toolchain_dir',
      default=os.path.join(PARENT_DIR, 'toolchain'),
      help='(optional) location of toolchain directory')
  parser.add_option(
      '--nacl-newlib-only', dest='filter_out_predicates',
      action='append_const', const=toolchainbinaries.IsNotNaClNewlibFlavor,
      help='download only the non-pnacl newlib toolchain')
  parser.add_option(
      '--allow-bionic', dest='allow_bionic', action='store_true',
      default=False,
      help='Allow download of bionic toolchain.')
  parser.add_option(
      '--no-pnacl', dest='filter_out_predicates', action='append_const',
      const=toolchainbinaries.IsPnaclFlavor,
      help='Filter out PNaCl toolchains.')
  parser.add_option(
      '--no-x86', dest='filter_out_predicates', action='append_const',
      const=toolchainbinaries.IsX86Flavor,
      help='Filter out x86 toolchains.')
  parser.add_option(
      '--arm-untrusted', dest='arm_untrusted', action='store_true',
      default=False, help='Add arm untrusted toolchains.')
  parser.add_option(
      '--no-pnacl-translator', dest='filter_out_predicates',
      action='append_const',
      const=toolchainbinaries.IsSandboxedTranslatorFlavor,
      help='Filter out PNaCl sandboxed translator.')
  parser.add_option(
      '--no-arm-trusted', dest='filter_out_predicates', action='append_const',
      const=toolchainbinaries.IsArmTrustedFlavor,
      help='Filter out trusted arm toolchains.')
  options, args = parser.parse_args(args)

  if not options.arm_untrusted:
    if options.filter_out_predicates is None:
      options.filter_out_predicates = []
    options.filter_out_predicates.append(toolchainbinaries.IsArmUntrustedFlavor)

  if not options.allow_bionic:
    if options.filter_out_predicates is None:
      options.filter_out_predicates = []
    options.filter_out_predicates.append(toolchainbinaries.IsBionicFlavor)

  if len(args) > 1:
    parser.error('Expecting only one version file.')
  return options, args


def ScriptDependencyTimestamp():
  """Determine the timestamp for the most recently changed script."""
  src_list = ['download_toolchains.py', '../pynacl/download_utils.py',
              '../pynacl/file_tools.py', '../pynacl/platform.py',
              'cygtar.py', '../pynacl/http_download.py']
  srcs = [os.path.join(SCRIPT_DIR, src) for src in src_list]
  src_times = []

  for src in srcs:
    src_times.append(os.stat(src).st_mtime)
  return sorted(src_times)[-1]


def main(args):
  script_time = ScriptDependencyTimestamp()
  options, version_files = ParseArgs(args)

  # If not provided, default to native_client/toolchain_versions.txt
  if not version_files:
    version_files = [os.path.join(PARENT_DIR, 'TOOL_REVISIONS')]
  versions = LoadVersions(version_files[0])

  if options.hashes:
    print '  (Calculating, may take a second...)'
    print '-' * 70
    sys.stdout.flush()
    ShowUpdatedDEPS(options, versions)
    print '-' * 70
    return 0

  host_os = pynacl.platform.GetOS()
  arch = pynacl.platform.GetArch()
  platform_mapping = toolchainbinaries.PLATFORM_MAPPING
  flavors = [flavor
             for flavor in platform_mapping[host_os][arch]
             if IsFlavorNeeded(options, flavor)]

  # Now that we have standardized the toolchain directories to a new format,
  # temporarily have code to delete the old directories to clean up machines.
  RemoveOldToolchainDir(options.toolchain_dir)

  for flavor in flavors:
    version = VersionSelect(versions, flavor)
    urls = FlavorUrls(options, versions, flavor)
    dst = FlavorOutDir(options, flavor)
    hashes = FlavorHashes(versions, flavor)
    flavor_name = FlavorName(flavor)

    if version == 'latest':
      print flavor + ': downloading latest version...'
      force = True
    else:
      force = False

    try:
      if SyncFlavor(flavor, urls, dst, hashes, script_time, force=force,
                    keep=options.keep, verbose=options.verbose):
        print flavor_name + ': updated to version ' + version + '.'
      else:
        print flavor_name + ': already up to date.'
    except pynacl.download_utils.HashError, e:
      print str(e)
      print '-' * 70
      print 'You probably want to update the %s hashes to:' % version_files[0]
      print '  (Calculating, may take a second...)'
      print '-' * 70
      sys.stdout.flush()
      ShowUpdatedDEPS(options, versions)
      print '-' * 70
      return 1
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
