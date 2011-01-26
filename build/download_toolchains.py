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
    'windows': [
        ['win_x86', 'win_x86'],  # Multilib toolchain
    ],
    'linux': [
        ['linux_x86', 'linux_x86'],  # Multilib toolchain
        ['linux_arm-trusted', 'linux_arm-trusted'],
        ['linux_arm-untrusted_hardy64', 'linux_arm-untrusted'],
    ],
    'mac': [
        ['mac_x86', 'mac_x86'], # Multilib toolchain
    ],
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
  for flavor in PLATFORM_MAPPING[platform_fixed]:
    if 'arm' in flavor[0]:
      version = options.arm_version
    else:
      version = options.x86_version
    url = '%s/%s/naclsdk_%s.tgz' % (options.base_url, version, flavor[0])
    parent_dir = os.path.dirname(os.path.dirname(__file__))
    dst = os.path.join(parent_dir, 'toolchain', flavor[1])
    if version != 'latest' and download_utils.SourceIsCurrent(dst, url):
      continue

    # TODO(bradnelson_): get rid of this when toolchain tarballs flattened.
    if 'arm' in flavor[0]:
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
