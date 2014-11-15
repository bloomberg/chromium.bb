#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script invokes the go build tool.
Must be called as follows:
python go.py <go-binary> <build directory> <output file> <src directory>
<CGO_CFLAGS> <CGO_LDFLAGS> <go-binary options>
eg.
python go.py /usr/lib/google-golang/bin/go out/build out/a.out .. "-I."
"-L. -ltest" test -c test/test.go
"""

import argparse
import os
import shutil
import sys

def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('go_binary')
  parser.add_argument('build_directory')
  parser.add_argument('output_file')
  parser.add_argument('src_root')
  parser.add_argument('cgo_cflags')
  parser.add_argument('cgo_ldflags')
  parser.add_argument('go_option', nargs='*')
  args = parser.parse_args()
  go_binary = args.go_binary
  build_dir = args.build_directory
  out_file = os.path.abspath(args.output_file)
  # The src directory specified is relative. We need this as an absolute path.
  src_root = os.path.abspath(args.src_root)
  # GOPATH must be absolute, and point to one directory up from |src_Root|
  go_path = os.path.abspath(os.path.join(src_root, ".."))
  # GOPATH also includes any third_party/go libraries that have been imported
  go_path += ":" +  os.path.abspath(os.path.join(src_root, "third_party/go"))
  go_options = args.go_option
  try:
    shutil.rmtree(build_dir, True)
    os.mkdir(build_dir)
  except:
    pass
  old_directory = os.getcwd()
  os.chdir(build_dir)
  os.environ["GOPATH"] = go_path
  os.environ["CGO_CFLAGS"] = args.cgo_cflags
  os.environ["CGO_LDFLAGS"] = args.cgo_ldflags
  os.system("%s %s" % (go_binary, " ".join(go_options)))
  out_files = [ f for f in os.listdir(".") if os.path.isfile(f)]
  if (len(out_files) > 0):
    shutil.move(out_files[0], out_file)
  os.chdir(old_directory)
  try:
    shutil.rmtree(build_dir, True)
  except:
    pass

if __name__ == '__main__':
  sys.exit(main())
