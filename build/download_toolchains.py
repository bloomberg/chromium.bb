#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Download all Native Client toolchains for this platform.

This module downloads multiple tgz's and expands them.
"""

import optparse
import os.path

import download_utils
import sync_tgz


PLATFORM_MAPPING = {
    'windows': {
        'x86-32': ['win_x86'],
        'x86-64': ['win_x86'],
    },
    'linux': {
        'x86-32': ['linux_x86','pnacl_linux_i686','linux_arm-trusted'],
        'x86-64': ['linux_x86','pnacl_linux_x86_64','linux_arm-trusted'],
        'arm'   : ['linux_x86','pnacl_linux_x86_64','linux_arm-trusted'],
    },
    'mac': {
        'x86-32': ['mac_x86'],
        'x86-64': ['mac_x86'],
    },
}

def main():
  parser = optparse.OptionParser()
  parser.add_option(
      '-b', '--base-url', dest='base_url',
      default=('http://commondatastorage.googleapis.com/'
               'nativeclient-archive2/toolchain'),
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
  for flavor in PLATFORM_MAPPING[platform_fixed][arch_fixed]:
    if 'arm' in flavor or 'pnacl' in flavor:
      version = options.arm_version
    else:
      version = options.x86_version
    url = '%s/%s/naclsdk_%s.tgz' % (options.base_url, version, flavor)
    parent_dir = os.path.dirname(os.path.dirname(__file__))
    dst = os.path.join(parent_dir, 'toolchain', flavor)
    if version != 'latest' and download_utils.SourceIsCurrent(dst, url):
      continue

    # TODO(bradnelson_): get rid of this when toolchain tarballs flattened.
    if 'arm' in flavor or 'pnacl' in flavor:
      # TODO(cbiffle): we really shouldn't do this until the unpack succeeds!
      # See: http://code.google.com/p/nativeclient/issues/detail?id=834
      download_utils.RemoveDir(dst)
      sync_tgz.SyncTgz(url, dst)
    else:
      dst_tmp = os.path.join(parent_dir, 'toolchain', '.tmp')
      sync_tgz.SyncTgz(url, dst_tmp)
      subdir = os.path.join(dst_tmp, 'sdk', 'nacl-sdk')
      download_utils.MoveDirCleanly(subdir, dst)
      download_utils.RemoveDir(dst_tmp)

    # Write out source url stamp.
    download_utils.WriteSourceStamp(dst, url)


if __name__ == '__main__':
  main()
