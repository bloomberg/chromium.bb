#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import shutil
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
src_build_dir = os.path.abspath(os.path.join(script_dir, os.pardir))
sys.path.insert(0, src_build_dir)

import vs_toolchain


def _HexDigest(file_name):
  hasher = hashlib.sha256()
  afile = open(file_name, 'rb')
  blocksize = 65536
  buf = afile.read(blocksize)
  while len(buf) > 0:
    hasher.update(buf)
    buf = afile.read(blocksize)
  afile.close()
  return hasher.hexdigest()


def _CopyImpl(file_name, target_dir, source_dir, verbose=False):
  """Copy |source| to |target| if it doesn't already exist or if it
  needs to be updated.
  """
  target = os.path.join(target_dir, file_name)
  source = os.path.join(source_dir, file_name)
  if (os.path.isdir(os.path.dirname(target)) and
      ((not os.path.isfile(target)) or
       _HexDigest(source) != _HexDigest(target))):
    if verbose:
      print 'Copying %s to %s...' % (source, target)
    if os.path.exists(target):
      os.unlink(target)
    shutil.copy(source, target)


def _CopyCDBToOutput(output_dir, target_arch):
  """Copies the Windows debugging executable cdb.exe to the output
  directory, which is created if it does not exist. The output
  directory, and target architecture that should be copied, are
  passed. Supported values for the target architecture are the GYP
  values "ia32" and "x64" and the GN values "x86" and "x64".
  """
  if not os.path.isdir(output_dir):
    os.makedirs(output_dir)
  vs_toolchain.SetEnvironmentAndGetRuntimeDllDirs()
  win_sdk_dir = os.path.normpath(os.environ['WINDOWSSDKDIR'])
  if target_arch == 'ia32' or target_arch == 'x86':
    src_arch = 'x86'
  elif target_arch == 'x64':
    src_arch = 'x64'
  else:
    print 'copy_cdb_to_output.py: unknown target_arch %s' % target_arch
    sys.exit(1)
  # We need to copy multiple files, so cache the computed source directory.
  src_dir = os.path.join(win_sdk_dir, 'Debuggers', src_arch)
  # Note that the outputs from the "copy_cdb_to_output" target need to
  # be kept in sync with this list.
  _CopyImpl('cdb.exe', output_dir, src_dir)
  _CopyImpl('dbgeng.dll', output_dir, src_dir)
  _CopyImpl('dbghelp.dll', output_dir, src_dir)
  _CopyImpl('dbgmodel.dll', output_dir, src_dir)
  return 0


def main():
  if len(sys.argv) < 2:
    print >>sys.stderr, 'Usage: copy_cdb_to_output.py <output_dir> ' + \
        '<target_arch>'
    return 1
  return _CopyCDBToOutput(sys.argv[1], sys.argv[2])


if __name__ == '__main__':
  sys.exit(main())
