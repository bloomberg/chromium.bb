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
        ['win_x86_win7', 'win_x86'],  # Multilib toolchain
    ],
    'linux': [
        ['linux_x86', 'linux_x86'],  # Multilib toolchain
        ['linux_arm-trusted', 'linux_arm-trusted'],
        ['linux_arm-untrusted_hardy64', 'linux_arm-untrusted'],
    ],
    'darwin': [
        ['mac_x86', 'mac_x86'], # Multilib toolchain
    ],
}


def Retry(op, *args):
  if sys.platform == 'win32':
    for i in range(0, 5):
      try:
        op(*args)
        break
      except:
        print "RETRY: ", op.__name__, args
        time.sleep(pow(2, i))
  else:
    op(*args)


def main(argv):
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
    source_url_file = os.path.join(dst, 'SOURCE_URL')
    if version != 'latest':
      try:
        fh = open(source_url_file, 'r')
        old_url = fh.read()
        fh.close()
        if old_url == url:
          continue
      except IOError:
        pass
    try:
      # TODO(cbiffle): we really shouldn't do this until the unpack succeeds!
      # See: http://code.google.com/p/nativeclient/issues/detail?id=834
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

    # Write out source url stamp.
    fh = open(os.path.join(dst, 'SOURCE_URL'), 'w')
    fh.write(url)
    fh.close()


if __name__ == '__main__':
  main(sys.argv[1:])
