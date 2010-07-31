#!/usr/bin/python
# Copyright 2010 The Native Client Authors.  All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

"""Download all Native Client toolchains for this platform.

This module downloads multiple tgz's and expands them.
"""

import optparse
import os
import shutil
import sys
import time

import sync_tgz


PLATFORM_COLLAPSE = {
    'win32': 'win32',
    'cygwin': 'win32',
    'linux': 'linux',
    'linux2': 'linux',
    'darwin': 'darwin',
}


PLATFORM_MAPPING = {
    'win32': [
        ['win_x86', 'win_x86a'],  # Multilib toolchain
    ],
    'linux': [
        ['linux_x86', 'linux_x86'],  # Multilib toolchain
        ['linux_arm-trusted', 'linux_arm-trusted'],
        ['linux_arm-untrusted', 'linux_arm-untrusted'],
    ],
    'darwin': [
        ['mac_x86', 'mac_x86'], # Multilib toolchain
    ],
}


def Retry(op, *args):
  for i in range(0, 10):
    try:
      op(*args)
      break
    except OSError:
      time.sleep(pow(2, i))


def main(argv):
  parser = optparse.OptionParser()
  parser.add_option(
      '-b', '--base-url', dest='base_url',
      default='http://build.chromium.org/buildbot/nacl_archive/nacl/toolchain',
      help='base url to download from')
  parser.add_option(
      '--x86-version', dest='x86_version',
      default='latest',
      help='which version of the toolchain to download for x86')
  parser.add_option(
      '--arm-version', dest='arm_version',
      default='latest',
      help='which version of the toolchain to download for arm')
  options, args = parser.parse_args(argv)
  if args:
    parser.print_help()
    print 'ERROR: invalid argument'
    sys.exit(1)

  platform_fixed = PLATFORM_COLLAPSE.get(sys.platform)
  for flavor in PLATFORM_MAPPING[platform_fixed]:
    if 'arm' in flavor[0]:
      version = options.arm_version
    else:
      version = options.x86_version
    url = '%s/%s/naclsdk_%s.tgz' % (
        options.base_url, version, flavor[0])
    parent_dir = os.path.dirname(os.path.dirname(__file__))
    dst = os.path.join(parent_dir, 'toolchain', flavor[1])
    try:
      shutil.rmtree(dst)
    except OSError:
      pass
    # TODO(bradnelson_): get rid of this when toolchain tarballs flattened.
    if 'arm' in flavor[0]:
      sync_tgz.SyncTgz(url, dst)
    else:
      dst_tmp = os.path.join(parent_dir, 'toolchain', '.tmp')
      sync_tgz.SyncTgz(url, dst_tmp)
      Retry(os.rename, os.path.join(dst_tmp, 'sdk', 'nacl-sdk'), dst)
      Retry(os.rmdir, os.path.join(dst_tmp, 'sdk'))
      Retry(os.rmdir, dst_tmp)


if __name__ == '__main__':
  main(sys.argv[1:])
