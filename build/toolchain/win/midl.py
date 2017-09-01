# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys


def main(arch, outdir, tlb, h, dlldata, iid, proxy, idl, *flags):
  # Read the environment block from the file. This is stored in the format used
  # by CreateProcess. Drop last 2 NULs, one for list terminator, one for
  # trailing vs. separator.
  env_pairs = open(arch).read()[:-2].split('\0')
  env_dict = dict([item.split('=', 1) for item in env_pairs])

  args = ['midl', '/nologo'] + list(flags) + [
      '/out', outdir,
      '/tlb', tlb,
      '/h', h,
      '/dlldata', dlldata,
      '/iid', iid,
      '/proxy', proxy,
      idl]
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
  return popen.returncode


if __name__ == '__main__':
  sys.exit(main(*sys.argv[1:]))
