#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import optparse
import os
import re
import shutil
import subprocess
import sys


# Where things are in relation to this script.
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.dirname(SCRIPT_DIR)
NACL_DIR = os.path.join(SRC_DIR, 'native_client')


def StripIRT(platform, src, dst):
  """Build the IRT for several platforms.

  Arguments:
    platform: is the name of the platform to build for.
    src: path to the input NEXE.
    dst: path to the output NEXE.
  """
  uplatform = platform.replace('-', '_')
  # NaCl Trusted code is in thumb2 mode in CrOS, but as yet,
  # untrusted code is still in classic ARM mode
  # arm-thumb2 is for the future when untrusted code is in thumb2 as well
  platform2 = {'arm': 'pnacl',
               'arm-thumb2' : 'pnacl',
               'x86-32': 'i686',
               'x86-64': 'x86_64'}.get(platform, uplatform)
  cplatform = {
      'win32': 'win',
      'cygwin': 'win',
      'darwin': 'mac',
  }.get(sys.platform, 'linux')
  if platform in ['arm', 'arm-thumb2']:
    cmd = [
        '../native_client/toolchain/pnacl_linux_x86_64_newlib/bin/' +
        platform2 + '-strip',
        '--strip-debug', src, '-o', dst
        ]
  else:
    cmd = [
        '../native_client/toolchain/' + cplatform + '_x86_newlib/bin/' +
        platform2 + '-nacl-strip',
        '--strip-debug', src, '-o', dst
        ]
  print 'Running: ' + ' '.join(cmd)
  p = subprocess.Popen(cmd, cwd=SCRIPT_DIR)
  p.wait()
  if p.returncode != 0:
    sys.exit(4)


def Main(argv):
  parser = optparse.OptionParser()
  parser.add_option('--platform', dest='platforms',
                    help='select a platform to strip')
  parser.add_option('--src', dest='src',
                    help='source IRT file')
  parser.add_option('--dst', dest='dst',
                    help='destination IRT file')
  (options, args) = parser.parse_args(argv[1:])
  if args or not options.platforms:
    parser.print_help()
    sys.exit(1)

  StripIRT(options.platforms, options.src, options.dst)


if __name__ == '__main__':
  Main(sys.argv)
