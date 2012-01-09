#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download all Native Client toolchains for this platform.

This module downloads multiple tgz's and expands them.
"""

import cygtar
import download_utils
import optparse
import os
import sys
import toolchainbinaries

def VersionSelect(options, flavor):
  """Decide which version to used based on options + flavor.

  Arguments:
    options: options from the command line.
    flavor: kind of toolchain.
  Returns:
    An svn version number.
  """
  if toolchainbinaries.IsArmTrustedFlavor(flavor):
    return options.arm_trusted_version
  elif toolchainbinaries.IsPnaclFlavor(flavor):
    return options.pnacl_version
  else:
    return options.x86_version


def GetUpdatedDEPS(options):
  """Return a suggested DEPS toolchain hash update for all platforms.

  Arguments:
    options: options from the command line.
  """
  flavors = set()
  for platform in toolchainbinaries.PLATFORM_MAPPING:
    pm = toolchainbinaries.PLATFORM_MAPPING[platform]
    for arch in pm:
      for flavor in pm[arch]:
        if (options.nacl_newlib_only and
            not toolchainbinaries.IsNaClNewlibFlavor(flavor)):
          continue
        if (options.no_pnacl and toolchainbinaries.IsPnaclFlavor(flavor)):
          continue
        if (options.no_arm_trusted and
            toolchainbinaries.IsArmTrustedFlavor(flavor)):
          continue
        flavors.add(flavor)
  new_deps = {}
  for flavor in flavors:
    url = toolchainbinaries.EncodeToolchainUrl(
      options.base_url, VersionSelect(options, flavor), flavor)
    new_deps['nacl_toolchain_%s_hash' % flavor] = download_utils.HashUrl(url)
  return new_deps


def ShowUpdatedDEPS(options):
  """Print a suggested DEPS toolchain hash update for all platforms.

  Arguments:
    options: options from the command line.
  """
  for key, value in sorted(GetUpdatedDEPS(options).iteritems()):
    print '  "%s":' % key
    print '      "%s",' % value
    sys.stdout.flush()


def SyncFlavor(flavor, url, dst, hash, min_time, keep=False, force=False,
               verbose=False):
  """Sync a flavor of the nacl toolchain

  Arguments:
    flavor: short directory name of the toolchain flavor.
    url: url to download the toolchain flavor from.
    dst: destination directory for the toolchain.
    hash: expected hash of the toolchain.
  """

  parent_dir = os.path.dirname(os.path.dirname(__file__))
  download_dir = os.path.join(parent_dir, 'toolchain', '.tars')
  untar_dir = os.path.join(parent_dir, 'toolchain', '.tmp')

  # Build the tarfile name from the url
  filepath = os.path.join(download_dir, url.split('/')[-1])

  # If we are forcing a sync, then ignore stamp
  if force:
    stamp_dir = None
  else:
    stamp_dir = dst

  # If we did not need to synchronize, then we are done
  if not download_utils.SyncURL(url, filepath, stamp_dir=stamp_dir,
                                min_time=min_time, hash=hash,
                                keep=keep, verbose=verbose):
    return False

  # If we didn't already have an expected hash (which must have matched the
  # actual hash), compute one so we can store it in the stamp file.
  if hash is None:
    hash = download_utils.HashFile(filepath)

  tar = cygtar.CygTar(filepath, 'r:*')
  curdir = os.getcwd()
  if not os.path.exists(untar_dir):
    os.makedirs(untar_dir)
  os.chdir(untar_dir)
  tar.Extract()
  tar.Close()
  os.chdir(curdir)

  if not keep:
    os.remove(filepath)

  # TODO(bradnelson_): get rid of this when toolchain tarballs flattened.
  if 'arm' in flavor or 'pnacl' in flavor:
    src = os.path.join(untar_dir)
  elif 'newlib' in flavor:
    src = os.path.join(untar_dir, 'sdk', 'nacl-sdk')
  else:
    src = os.path.join(untar_dir, 'toolchain', flavor)

  # Move and update the stamp
  download_utils.MoveDirCleanly(src ,dst)
  download_utils.RemoveDir(untar_dir)
  download_utils.WriteSourceStamp(dst, url)
  download_utils.WriteHashStamp(dst, hash)
  return True


def Main(args):
 # Generate the time for the most recently modified script used by the download
  script_dir = os.path.dirname(__file__)
  src_list = ['download_toolchains.py', 'download_utils.py',
              'cygtar.py', 'http_download.py']
  srcs = [os.path.join(script_dir, src) for src in src_list]
  src_times = []
  for src in srcs:
    src_times.append( os.stat(src).st_mtime )
  script_time = sorted(src_times)[-1]

  parent_dir = os.path.dirname(script_dir)
  parser = optparse.OptionParser()
  parser.add_option(
      '-b', '--base-url', dest='base_url',
      default=toolchainbinaries.BASE_DOWNLOAD_URL,
      help='base url to download from')
  parser.add_option(
      '-k', '--keep', dest='keep',
      default=False,
      action='store_true',
      help='Keep the downloaded tarballs.')
  parser.add_option(
      '-v', '--verbose', dest='verbose',
      default=False,
      action='store_true',
      help='Use verbose output.')
  parser.add_option(
      '--x86-version', dest='x86_version',
      default='latest',
      help='which version of the toolchain to download for x86')
  parser.add_option(
      '--arm-trusted-version', dest='arm_trusted_version',
      default='latest',
      help='which version of the trusted toolchain to download for arm')
  parser.add_option(
      '--pnacl-version', dest='pnacl_version',
      default='latest',
      help='which version of the toolchain to download for pnacl')
  parser.add_option(
      '--toolchain-dir', dest='toolchain_dir',
      default=os.path.join(parent_dir, 'toolchain'),
      help='(optional) location of toolchain directory')
  parser.add_option(
      '--file-hash', dest='file_hashes', action='append', nargs=2, default=[],
      metavar='ARCH HASH',
      help='ARCH gives the name of the architecture (e.g. "x86_32") for '
           'which to download an IRT binary.  '
           'HASH gives the expected SHA1 hash of the file.')
  parser.add_option(
      '--nacl-newlib-only', dest='nacl_newlib_only',
      action='store_true', default=False,
      help='download only the non-pnacl newlib toolchain')
  parser.add_option(
      '--save-downloads-dir', dest='save_downloads_dir',
      default=None,
      help='(optional) preserve the toolchain archives to this dir')
  parser.add_option(
      '--no-pnacl', dest='no_pnacl', default=False, action='store_true',
      help='Filter out PNaCl toolchains.')
  parser.add_option(
      '--no-arm-trusted', dest='no_arm_trusted', default=False,
      action='store_true', help='Filter out trusted arm toolchains.')

  options, args = parser.parse_args(args)
  if args:
    parser.error('ERROR: invalid argument')

  platform_fixed = download_utils.PlatformName()
  arch_fixed = download_utils.ArchName()
  flavors = toolchainbinaries.PLATFORM_MAPPING[platform_fixed][arch_fixed]
  if options.no_pnacl:
    flavors = [flavor for flavor in flavors
               if not toolchainbinaries.IsPnaclFlavor(flavor)]
  if options.no_arm_trusted:
    flavors = [flavor for flavor in flavors
               if not toolchainbinaries.IsArmTrustedFlavor(flavor)]
  if options.nacl_newlib_only:
    flavors = [flavor for flavor in flavors
               if toolchainbinaries.IsNaClNewlibFlavor(flavor)]
  for flavor in flavors:
    version = VersionSelect(options, flavor)
    url = toolchainbinaries.EncodeToolchainUrl(
      options.base_url, version, flavor)
    dst = os.path.join(options.toolchain_dir, flavor)
    if version == 'latest':
      print flavor + ': downloading latest version...'
      force = True
    else:
      force = False

    # If there are any hashes listed, check and require them for every flavor.
    # List a bogus hash if none is specified so we get an error listing the
    # correct hash if its missing.
    if options.file_hashes:
      hash_value = 'unspecified'  # bogus default.
      for arch, hval in options.file_hashes:
        if arch == flavor:
          hash_value = hval
          break
    else:
      hash_value = None

    try:
      if SyncFlavor(flavor, url, dst, hash_value, script_time, force=force,
                    keep=options.keep, verbose=options.verbose):
        print flavor + ': updated to version ' + version + '.'
      else:
        print flavor + ': already up to date.'
    except download_utils.HashError, e:
      print str(e)
      print '-' * 70
      print 'You probably want to update the DEPS hashes to:'
      print '  (Calculating, may take a second...)'
      print '-' * 70
      sys.stdout.flush()
      ShowUpdatedDEPS(options)
      print '-' * 70
      sys.exit(1)


if __name__ == '__main__':
  Main(sys.argv[1:])
