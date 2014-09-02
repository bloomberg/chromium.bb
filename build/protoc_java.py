#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generate java source files from protobuf files.

Usage:
    protoc_java.py {protoc} {proto_path} {java_out} {proto_runtime} \
        {stamp_file} {proto_files}

This is a helper file for the genproto_java action in protoc_java.gypi.

It performs the following steps:
1. Deletes all old sources (ensures deleted classes are not part of new jars).
2. Creates source directory.
3. Generates Java files using protoc.
4. Creates a new stamp file.

proto_runtime must be one of 'nano' and 'lite'.
"""

import os
import shutil
import subprocess
import sys

def main(argv):
  if len(argv) < 6:
    usage()
    return 1

  protoc_path, proto_path, java_out, proto_runtime, stamp_file = argv[1:6]
  proto_files = argv[6:]

  # Delete all old sources.
  if os.path.exists(java_out):
    shutil.rmtree(java_out)

  # Create source directory.
  os.makedirs(java_out)

  # Figure out which runtime to use.
  if proto_runtime == 'nano':
    out_arg = '--javanano_out=optional_field_style=reftypes,' + \
        'store_unknown_fields=true:' + java_out
  elif proto_runtime == 'lite':
    out_arg = '--java_out=' + java_out
  else:
    usage()
    return 1

  # Generate Java files using protoc.
  ret = subprocess.call(
      [protoc_path, '--proto_path', proto_path, out_arg] + proto_files)

  if ret == 0:
    # Create a new stamp file.
    with file(stamp_file, 'a'):
      os.utime(stamp_file, None)

  return ret

def usage():
  print(__doc__);

if __name__ == '__main__':
  sys.exit(main(sys.argv))
