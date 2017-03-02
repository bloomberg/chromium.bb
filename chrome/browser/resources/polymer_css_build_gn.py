#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import os
import sys

_HERE_PATH = os.path.dirname(__file__)
_SRC_PATH = os.path.normpath(os.path.join(_HERE_PATH, '..', '..', '..'))
_CWD = os.getcwd()

sys.path.append(os.path.join(_SRC_PATH, 'third_party', 'node'))
import node
import node_modules

def _css_build(out_folder, input_files, output_files):
  out_path = os.path.join(_CWD, out_folder)
  in_paths = map(lambda f: os.path.join(out_path, f), input_files)
  out_paths = map(lambda f: os.path.join(out_path, f), output_files)

  args = ['--no-inline-includes', '-f'] + in_paths + ['-o'] + out_paths
  node.RunNode([node_modules.PathToPolymerCssBuild()] + args)


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--out_folder')
  parser.add_argument('--input_files', nargs='+')
  parser.add_argument('--output_files', nargs='+')
  args = parser.parse_args()

  _css_build(
      args.out_folder,
      input_files=args.input_files,
      output_files=args.output_files)


if __name__ == '__main__':
  main()
