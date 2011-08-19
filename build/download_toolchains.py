#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Download all Native Client toolchains for this platform.

This module downloads multiple tgz's and expands them.
"""

import optparse
import os.path

import download_utils
import toolchainbinaries
import sync_tgz
import sys


def ShowUpdatedDEPS(base_url, version, nacl_newlib_only):
  """Print a suggested DEPS toolchain hash update for all platforms.

  Arguments:
    base_url: base download url for the toolchains.
    version: revision of the toolchain to use.
    nacl_newlib_only: flag indicating to only consider non-pnacl newlib flavors.
  """
  flavors = set()
  for platform in toolchainbinaries.PLATFORM_MAPPING:
    pm = toolchainbinaries.PLATFORM_MAPPING[platform]
    for arch in pm:
      for flavor in pm[arch]:
        if (nacl_newlib_only and
            not toolchainbinaries.IsNaClNewlibFlavor(flavor)):
          continue
        flavors.add(flavor)
  for flavor in flavors:
    url = toolchainbinaries.EncodeToolchainUrl(base_url, version, flavor)
    print '  "nacl_toolchain_%s_hash":' % flavor
    print '      "%s",' % sync_tgz.HashUrl(url)
    sys.stdout.flush()


def SyncFlavor(flavor, url, dst, hash):
  """Sync a flavor of the nacl toolchain

  Arguments:
    flavor: short directory name of the toolchain flavor.
    url: url to download the toolchain flavor from.
    dst: destination directory for the toolchain.
    hash: expected hash of the toolchain.
  """
  parent_dir = os.path.dirname(os.path.dirname(__file__))
  # TODO(bradnelson_): get rid of this when toolchain tarballs flattened.
  if 'arm' in flavor or 'pnacl' in flavor:
    # TODO(cbiffle): we really shouldn't do this until the unpack succeeds!
    # See: http://code.google.com/p/nativeclient/issues/detail?id=834
    download_utils.RemoveDir(dst)
    sync_tgz.SyncTgz(url, dst, verbose=False, hash=hash)
  elif 'newlib' in flavor:
    dst_tmp = os.path.join(parent_dir, 'toolchain', '.tmp')
    sync_tgz.SyncTgz(url, dst_tmp, verbose=False, hash=hash)
    subdir = os.path.join(dst_tmp, 'sdk', 'nacl-sdk')
    download_utils.MoveDirCleanly(subdir, dst)
    download_utils.RemoveDir(dst_tmp)
  else:
    dst_tmp = os.path.join(parent_dir, 'toolchain', '.tmp')
    sync_tgz.SyncTgz(url, dst_tmp, maindir='toolchain', verbose=False,
                     hash=hash)
    subdir = os.path.join(dst_tmp, 'toolchain', flavor)
    download_utils.MoveDirCleanly(subdir, dst)
    download_utils.RemoveDir(dst_tmp)

  # Write out source url stamp.
  download_utils.WriteSourceStamp(dst, url)


def Main():
  parser = optparse.OptionParser()
  parser.add_option(
      '-b', '--base-url', dest='base_url',
      default=toolchainbinaries.BASE_DOWNLOAD_URL,
      help='base url to download from')
  parser.add_option(
      '--x86-version', dest='x86_version',
      default='latest',
      help='which version of the toolchain to download for x86')
  parser.add_option(
      '--arm-version', dest='arm_version',
      default='latest',
      help='which version of the toolchain to download for arm')
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

  options, args = parser.parse_args()
  if args:
    parser.error('ERROR: invalid argument')

  platform_fixed = download_utils.PlatformName()
  arch_fixed = download_utils.ArchName()
  flavors = toolchainbinaries.PLATFORM_MAPPING[platform_fixed][arch_fixed]
  if options.nacl_newlib_only:
    flavors = [f for f in flavors if toolchainbinaries.IsNaClNewlibFlavor(f)]
  for flavor in flavors:
    if toolchainbinaries.IsArmFlavor(flavor):
      version = options.arm_version
    else:
      version = options.x86_version
    url = toolchainbinaries.EncodeToolchainUrl(options.base_url, version,
                                               flavor)
    parent_dir = os.path.dirname(os.path.dirname(__file__))
    dst = os.path.join(parent_dir, 'toolchain', flavor)
    if version == 'latest':
      print flavor + ': downloading latest version...'
    else:
      msg = download_utils.SourceIsCurrent(dst, url)
      if msg:
        print flavor + ': ' + msg + '..'
        continue
      else:
        print flavor + ': downloading version ' + version + '...'

    # If there are any hashes listed, check and require them for every flavor.
    # List a bogus hash if none is specified so we get an error listing the
    # correct hash if its missing.
    if options.file_hashes:
      hash = 'unspecified'  # bogus default.
      for arch, hval in options.file_hashes:
        if arch == flavor:
          hash = hval
          break
    else:
      hash = None

    try:
      SyncFlavor(flavor, url, dst, hash)
    except sync_tgz.HashError, e:
      print str(e)
      print '-' * 70
      print 'You probably want to update the DEPS hashes to:'
      print '  (Calculating, may take a second...)'
      print '-' * 70
      sys.stdout.flush()
      ShowUpdatedDEPS(options.base_url, options.x86_version,
                      options.nacl_newlib_only)
      print '-' * 70
      sys.exit(1)


if __name__ == '__main__':
  Main()
