#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from optparse import OptionParser, OptionValueError
import os
import shutil
import sys

"""Prep NaCl Toolchain

Does final assembly of the NaCl toolchain prior to building untrusted modules.
"""

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
NACL_DIR = os.path.dirname(SCRIPT_DIR)

# Mapping from TC/include relative path to original source
HEADER_MAP = {
  'x86_newlib': {
      'x86_64-nacl/include/pthread.h': 'src/untrusted/pthread/pthread.h',
      'x86_64-nacl/include/semaphore.h': 'src/untrusted/pthread/semaphore.h',
      'x86_64-nacl/include/nacl/dynamic_annotations.h':
          'src/untrusted/valgrind/dynamic_annotations.h',
      'x86_64-nacl/include/nacl/nacl_dyncode.h':
          'src/untrusted/nacl/nacl_dyncode.h',
      'x86_64-nacl/include/nacl/nacl_random.h':
          'src/untrusted/nacl/nacl_random.h',
      'x86_64-nacl/include/nacl/nacl_startup.h':
          'src/untrusted/nacl/nacl_startup.h',
      'x86_64-nacl/include/nacl/nacl_thread.h':
          'src/untrusted/nacl/nacl_thread.h',
      'x86_64-nacl/include/irt.h': 'src/untrusted/irt/irt.h',
      'x86_64-nacl/include/irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',

      'x86_64-nacl/lib32/crt1.o': 'src/untrusted/stubs/crt1.x',
      'x86_64-nacl/lib/crt1.o': 'src/untrusted/stubs/crt1.x',
  },
  'x86_glibc': {
      'x86_64-nacl/include/nacl/dynamic_annotations.h':
          'src/untrusted/valgrind/dynamic_annotations.h',
      'x86_64-nacl/include/nacl/nacl_dyncode.h':
          'src/untrusted/nacl/nacl_dyncode.h',
      'x86_64-nacl/include/nacl/nacl_random.h':
          'src/untrusted/nacl/nacl_random.h',
      'x86_64-nacl/include/nacl/nacl_startup.h':
          'src/untrusted/nacl/nacl_startup.h',
      'x86_64-nacl/include/nacl/nacl_thread.h':
          'src/untrusted/nacl/nacl_thread.h',
      'x86_64-nacl/include/irt.h': 'src/untrusted/irt/irt.h',
      'x86_64-nacl/include/irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
  },
  'arm_newlib': {
      'arm-nacl/include/pthread.h': 'src/untrusted/pthread/pthread.h',
      'arm-nacl/include/semaphore.h': 'src/untrusted/pthread/semaphore.h',
      'arm-nacl/include/nacl/dynamic_annotations.h':
          'src/untrusted/valgrind/dynamic_annotations.h',
      'arm-nacl/include/nacl/nacl_dyncode.h':
          'src/untrusted/nacl/nacl_dyncode.h',
      'arm-nacl/include/nacl/nacl_random.h':
          'src/untrusted/nacl/nacl_random.h',
      'arm-nacl/include/nacl/nacl_startup.h':
          'src/untrusted/nacl/nacl_startup.h',
      'arm-nacl/include/nacl/nacl_thread.h':
          'src/untrusted/nacl/nacl_thread.h',
      'arm-nacl/include/irt.h': 'src/untrusted/irt/irt.h',
      'arm-nacl/include/irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
      'arm-nacl/lib/crt1.o': 'src/untrusted/stubs/crt1.x',
  },
  'pnacl': {
      'sdk/include/pthread.h': 'src/untrusted/pthread/pthread.h',
      'sdk/include/semaphore.h': 'src/untrusted/pthread/semaphore.h',
      'sdk/include/nacl/dynamic_annotations.h':
          'src/untrusted/valgrind/dynamic_annotations.h',
      'sdk/include/nacl/nacl_dyncode.h':
          'src/untrusted/nacl/nacl_dyncode.h',
      'sdk/include/nacl/nacl_random.h':
          'src/untrusted/nacl/nacl_random.h',
      'sdk/include/nacl/nacl_startup.h':
          'src/untrusted/nacl/nacl_startup.h',
      'sdk/include/nacl/nacl_thread.h':
          'src/untrusted/nacl/nacl_thread.h',
      'sdk/include/irt.h': 'src/untrusted/irt/irt.h',
      'sdk/include/irt_ppapi.h': 'src/untrusted/irt/irt_ppapi.h',
  }
}


def PosixPath(filepath):
  if sys.platform == 'win32':
    return filepath.replace('\\', '/')
  return filepath


def RealToRelative(filepath, basepath):
  """Returns a relative path from an absolute basepath and filepath."""
  path_parts = filepath.split('/')
  base_parts = basepath.split('/')
  while path_parts and base_parts and path_parts[0] == base_parts[0]:
    path_parts = path_parts[1:]
    base_parts = base_parts[1:]
  rel_parts = ['..'] * len(base_parts) + path_parts
  return '/'.join(rel_parts)


def MakeDir(outdir):
  if outdir and not os.path.exists(outdir):
    os.makedirs(outdir)


def GetInputs(tc_map, basepath):
  out_set = set()
  for filename in tc_map:
    src = PosixPath(os.path.join(NACL_DIR, tc_map[filename]))
    out_set.add(RealToRelative(src, basepath))

  return '\n'.join(sorted(out_set)) + '\n'


def CopyFiles(tc_map, tc_dst):
  for filename in tc_map:
    src = PosixPath(os.path.join(NACL_DIR, tc_map[filename]))
    dst = os.path.join(tc_dst, filename)
    MakeDir(os.path.dirname(dst))
    if not os.path.exists(src):
      raise RuntimeError('File not found: ' + src)

    shutil.copy(src, dst)


def DoMain(argv):
  parser = OptionParser()
  parser.add_option('-p', '--path', dest='path', default='',
                    help='Set include path.')
  parser.add_option('-t', '--tool', dest='tool', default='',
                    help='Select toolset.')
  parser.add_option('-i', '--inputs', dest='inputs', action='store_true',
                    help='Return dependency list.', default=False)
  parser.add_option('-v', '--verbose', dest='verbose', action='store_true',
                    help='Enable versbose output.')

  options, files = parser.parse_args(argv)
  basepath = PosixPath(os.path.abspath(os.getcwd()))

  if len(files):
    raise OptionValueError('Not expecting arguments: ' + ' '.join(files))

  if options.tool not in HEADER_MAP:
    raise OptionValueError('Unknown tool name: ' + options.tool)

  tc_map = HEADER_MAP[options.tool]
  tc_dest = os.path.abspath(options.path)

  if options.inputs:
    return GetInputs(tc_map, basepath)

  if not options.path or not os.path.exists(options.path):
    raise RuntimeError('Must set path to a valid directory.')

  if not options.tool in HEADER_MAP:
    raise RuntimeError('Must specify the toolchain.')

  if not options.inputs:
    CopyFiles(tc_map, tc_dest)
    fh = open(os.path.join(tc_dest, 'stamp.prep'), 'w')
    fh.write('NACL SDK stamp for %s\n' % options.tool)
    fh.close()
    if options.verbose:
      print 'Created stamp file.'
  return 0


def main(argv):
  result = DoMain(argv[1:])
  if type(result) == int:
    return result

  sys.stdout.write(result)
  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
