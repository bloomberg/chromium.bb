#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser, OptionValueError
import os
import shutil
import subprocess
import sys

BUILD_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(BUILD_DIR)

sys.path.append(os.path.join(NACL_DIR, 'toolchain_build'))
from file_update import Mkdir, UpdateFromTo, NeedsUpdate

"""Prep NaCl Toolchain

Does final assembly of the NaCl toolchain prior to building untrusted modules.
"""

#
# TODO(dyen) : breakout function from package_version to get
# paths (see GetRawToolchainSource bellow).
#
# TODO(noelallen) : Share HEADER_MAP between this and build_sdk.
# note:  missing ppapi files.
#

# Mapping from TC/include relative path to original source
# List of toolchain headers to install.
# Source is relative to top of Chromium tree, destination is relative
# to the toolchain header directory.
NACL_HEADER_MAP = {
  'newlib': [
      ('src/include/nacl/nacl_exception.h', 'nacl/nacl_exception.h'),
      ('src/include/nacl/nacl_minidump.h', 'nacl/nacl_minidump.h'),
      ('src/untrusted/irt/irt.h', 'irt.h'),
      ('src/untrusted/irt/irt_dev.h', 'irt_dev.h'),
      ('src/untrusted/nacl/nacl_dyncode.h', 'nacl/nacl_dyncode.h'),
      ('src/untrusted/nacl/nacl_startup.h', 'nacl/nacl_startup.h'),
      ('src/untrusted/nacl/nacl_thread.h', 'nacl/nacl_thread.h'),
      ('src/untrusted/pthread/pthread.h', 'pthread.h'),
      ('src/untrusted/pthread/semaphore.h', 'semaphore.h'),
      ('src/untrusted/valgrind/dynamic_annotations.h',
          'nacl/dynamic_annotations.h'),
  ],
  'glibc': [
      ('src/include/nacl/nacl_exception.h', 'nacl/nacl_exception.h'),
      ('src/include/nacl/nacl_minidump.h', 'nacl/nacl_minidump.h'),
      ('src/untrusted/irt/irt.h', 'irt.h'),
      ('src/untrusted/irt/irt_dev.h', 'irt_dev.h'),
      ('src/untrusted/nacl/nacl_dyncode.h', 'nacl/nacl_dyncode.h'),
      ('src/untrusted/nacl/nacl_startup.h', 'nacl/nacl_startup.h'),
      ('src/untrusted/nacl/nacl_thread.h', 'nacl/nacl_thread.h'),
      ('src/untrusted/valgrind/dynamic_annotations.h',
            'nacl/dynamic_annotations.h'),
  ],
  'host': []
}

NACL_CRT_MAP = {
  'i686': [
      ('src/untrusted/stubs/crti_x86_32.S', 'x86_64-nacl/lib32/crti.o'),
      ('src/untrusted/stubs/crtn_x86_32.S', 'x86_64-nacl/lib32/crtn.o'),
      ('src/untrusted/stubs/crt1.x', 'x86_64-nacl/lib32/crt1.o'),
  ],
  'x86_64': [
      ('src/untrusted/stubs/crti_x86_64.S', 'x86_64-nacl/lib/crti.o'),
      ('src/untrusted/stubs/crtn_x86_64.S', 'x86_64-nacl/lib/crtn.o'),
      ('src/untrusted/stubs/crt1.x', 'x86_64-nacl/lib/crt1.o'),
  ],
  'arm': [
      ('src/untrusted/stubs/crti_arm.S', 'arm-nacl/lib/crti.o'),
      ('src/untrusted/stubs/crtn_arm.S', 'arm-nacl/lib/crtn.o'),
      ('src/untrusted/stubs/crt1.x', 'arm-nacl/lib32/crt1.o'),
  ],
  'pnacl' : [],
}



#
# Share package_version code for finding paths...
#

def GetPlatform():
  if sys.platform.startswith('cygwin') or sys.platform.startswith('win'):
    return 'win'
  elif sys.platform.startswith('darwin'):
    return 'mac'
  elif sys.platform.startswith('linux'):
    return 'linux'
  else:
    raise Error("Unknown platform: %s" % sys.platform)


def UpdateNaClHeaders(out_path, libc, arch):
  modified = False
  for src, dst in NACL_HEADER_MAP[libc]:
    src_path = os.path.join(NACL_DIR, src)
    dst_path = os.path.join(out_path, arch + '-nacl', 'include', dst)
    modifed = modified or UpdateFromTo(src_path, dst_path)
  return modified


def CompileCrt(out_path, libc, sub_arches):
  modified = False
  for sub_arch in sub_arches:
    cc = os.path.join(out_path, 'bin', sub_arch + '-nacl-gcc')
    for src, dst in NACL_CRT_MAP[sub_arch]:
      src_path = os.path.join(NACL_DIR, src)
      dst_path = os.path.join(out_path, dst)
      if not NeedsUpdate(src_path, dst_path):
        continue
      if src.endswith('.x'):
        UpdateFromTo(src_path, dst_path)
      else:
        args = [cc, '-c', src_path, '-o', dst_path]
        subprocess.call(args)
      modified = True
  return modified


def UpdateToolchain(out_root, libc, arch, sub_arches):
  modified = UpdateNaClHeaders(out_root, libc, arch)
  modified |= CompileCrt(out_root, libc, sub_arches)
  # Add default DEP to this STAMP FILE for this toolchain
  return modified


PACKAGE_MAP = {
  'nacl_x86_glibc': ('x86_64', ['x86_64', 'i686'], 'glibc'),
  'nacl_x86_newlib': ('x86_64', ['x86_64', 'i686'], 'newlib'),
  'nacl_arm_newlib': ('arm', ['arm'], 'newlib'),
  'pnacl_newlib' : ('pnacl', ['pnacl'], 'newlib'),
}


def main(argv):
  print("Called GN_PREP_TOOLCHAIN: " + ' '.join(argv))

  parser = OptionParser()
  parser.add_option('-p', '--package', dest='package', default=None,
                    help='Set include path.')
  parser.add_option('-t', '--toolchain-dir', dest='toolchain_dir', default=None,
                    help='Provide toolchain path.')
  parser.add_option('-s', '--stamp', dest='stamp', default=None,
                    help='Location of stamp file to update.')
  options, files = parser.parse_args(argv)

  if not options.package:
    parser.error('Requires package name.')
  if not options.toolchain_dir:
    parser.error('Requires toolchain install dir.')
  if files:
    parser.error('Not expecting extra arguments: ' + ' '.join(files))

  arch, sub_arches, libc = PACKAGE_MAP[options.package]
  modified = UpdateToolchain(options.toolchain_dir, libc, arch, sub_arches)
  if modified and options.stamp:
    fh = open(os.path.join(options.stamp), 'w')
    fh.write('NACL SDK stamp for %s\n' % options.package)
    fh.close()

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
