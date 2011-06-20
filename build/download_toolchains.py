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
  options, args = parser.parse_args()
  if args:
    parser.error('ERROR: invalid argument')

  platform_fixed = download_utils.PlatformName()
  arch_fixed = download_utils.ArchName()
  for flavor in toolchainbinaries.PLATFORM_MAPPING[platform_fixed][arch_fixed]:
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

    # TODO(bradnelson_): get rid of this when toolchain tarballs flattened.
    if 'arm' in flavor or 'pnacl' in flavor:
      # TODO(cbiffle): we really shouldn't do this until the unpack succeeds!
      # See: http://code.google.com/p/nativeclient/issues/detail?id=834
      download_utils.RemoveDir(dst)
      sync_tgz.SyncTgz(url, dst, verbose=False)
    elif 'newlib' in flavor:
      dst_tmp = os.path.join(parent_dir, 'toolchain', '.tmp')
      sync_tgz.SyncTgz(url, dst_tmp, verbose=False)
      subdir = os.path.join(dst_tmp, 'sdk', 'nacl-sdk')
      download_utils.MoveDirCleanly(subdir, dst)
      download_utils.RemoveDir(dst_tmp)
    else:
      dst_tmp = os.path.join(parent_dir, 'toolchain', '.tmp')
      if sys.platform == 'win32':
        sync_tgz.SyncTgz(url, dst_tmp,
                         compress='xz', maindir='toolchain', verbose=False)
      else:
        sync_tgz.SyncTgz(url, dst_tmp, maindir='toolchain', verbose=False)
      subdir = os.path.join(dst_tmp, 'toolchain', flavor)
      download_utils.MoveDirCleanly(subdir, dst)
      download_utils.RemoveDir(dst_tmp)

    # Write out source url stamp.
    download_utils.WriteSourceStamp(dst, url)


if __name__ == '__main__':
  Main()
