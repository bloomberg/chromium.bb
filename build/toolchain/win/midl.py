# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import difflib
import distutils.dir_util
import filecmp
import os
import re
import shutil
import subprocess
import sys
import tempfile


def ZapTimestamp(filename):
  # midl.exe writes timestamp 2147483647 (2^31 - 1) as creation date into its
  # outputs, but using the local timezone.  To make the output timezone-
  # independent, replace that date with a fixed string of the same length.
  if filename.endswith('.tlb'):
    lookbehind = 'Created by MIDL version 8\.01\.0622 '
  else:
    lookbehind = 'File created by MIDL compiler version 8\.01\.0622 \*/\r\n/\* '
  contents = open(filename, 'rb').read()
  contents = re.sub(r'(?<=%s)at ... Jan 1. ..:..:.. 2038' % lookbehind,
                            'at a redacted point in time', contents)
  open(filename, 'wb').write(contents)


def main(arch, outdir, tlb, h, dlldata, iid, proxy, idl, *flags):
  # chromoting_lib.idl uses a uuid that's hashed of chrome's version string,
  # i.e. it changes every few compiles.  So a checked-in file does not work
  # for chromoting_lib.idl.  For now, call midl.exe for remoting instead of
  # using checked-in artifacts for it.
  is_chromoting = os.path.basename(idl) == 'chromoting_lib.idl'

  # Copy checked-in outputs to final location.
  THIS_DIR = os.path.abspath(os.path.dirname(__file__))
  source = os.path.join(THIS_DIR, "..", "..", "..",
      "third_party", "win_build_output", outdir.replace('gen/', 'midl/'))
  if os.path.isdir(os.path.join(source, os.path.basename(idl))):
    source = os.path.join(source, os.path.basename(idl))
  source = os.path.join(source, arch.split('.')[1])  # Append 'x86' or 'x64'.
  if not is_chromoting:
    distutils.dir_util.copy_tree(source, outdir, preserve_times=False)

  # On non-Windows, that's all we can do.
  if sys.platform != 'win32':
    return 0 if not is_chromoting else 1

  # On Windows, run midl.exe on the input and check that its outputs are
  # identical to the checked-in outputs.
  if not is_chromoting:
    tmp_dir = tempfile.mkdtemp()
    delete_tmp_dir = True
  else:
    tmp_dir = outdir
    delete_tmp_dir = False

  # Read the environment block from the file. This is stored in the format used
  # by CreateProcess. Drop last 2 NULs, one for list terminator, one for
  # trailing vs. separator.
  env_pairs = open(arch).read()[:-2].split('\0')
  env_dict = dict([item.split('=', 1) for item in env_pairs])

  args = ['midl', '/nologo'] + list(flags) + [
      '/out', tmp_dir,
      '/tlb', tlb,
      '/h', h,
      '/dlldata', dlldata,
      '/iid', iid,
      '/proxy', proxy,
      idl]
  try:
    popen = subprocess.Popen(args, shell=True, env=env_dict,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out, _ = popen.communicate()
    # Filter junk out of stdout, and write filtered versions. Output we want
    # to filter is pairs of lines that look like this:
    # Processing C:\Program Files (x86)\Microsoft SDKs\...\include\objidl.idl
    # objidl.idl
    lines = out.splitlines()
    prefixes = ('Processing ', '64 bit Processing ')
    processing = set(os.path.basename(x)
                     for x in lines if x.startswith(prefixes))
    for line in lines:
      if not line.startswith(prefixes) and line not in processing:
        print line
    if popen.returncode != 0:
      return popen.returncode
    if is_chromoting:
      return 0

    for f in os.listdir(tmp_dir):
      ZapTimestamp(os.path.join(tmp_dir, f))

    # Now compare the output in tmp_dir to the checked-in outputs.
    diff = filecmp.dircmp(tmp_dir, source)
    if diff.diff_files or set(diff.left_list) != set(diff.right_list):
      print 'midl.exe output different from files in %s, see %s' \
          % (source, tmp_dir)
      diff.report()
      for f in diff.diff_files:
        if f.endswith('.tlb'): continue
        fromfile = os.path.join(source, f)
        tofile = os.path.join(tmp_dir, f)
        print ''.join(difflib.unified_diff(open(fromfile, 'U').readlines(),
                                           open(tofile, 'U').readlines(),
                                           fromfile, tofile))
      delete_tmp_dir = False
      sys.exit(1)
    return 0
  finally:
    if os.path.exists(tmp_dir) and delete_tmp_dir:
      shutil.rmtree(tmp_dir)


if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
