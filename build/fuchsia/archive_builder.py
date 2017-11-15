#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages a tar.gz archive of a binary along with its dependencies. This
contains the Chromium parts of what would normally be added to the bootfs
used to boot QEMU or a device."""

import argparse
import os
import sys

from runner_common import BuildArchive, ReadRuntimeDeps, ImageCreationData


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--output-directory',
                      type=os.path.realpath,
                      help=('Path to the directory in which build files are'
                            ' located (must include build type).'))
  parser.add_argument('--runtime-deps-path',
                      type=os.path.realpath,
                      help='Runtime data dependency file from GN.')
  parser.add_argument('--target-cpu',
                      help='GN target_cpu setting for the build.')
  parser.add_argument('--exe-name',
                      type=os.path.realpath,
                      help='Name of the the binary executable.')
  parser.add_argument('--no-autorun', action='store_true',
                      help='Disable generating an autorun file')
  args, child_args = parser.parse_known_args()

  data = ImageCreationData(output_directory=args.output_directory,
                           exe_name=args.exe_name,
                           runtime_deps=ReadRuntimeDeps(
                               args.runtime_deps_path, args.output_directory),
                           target_cpu=args.target_cpu)
  BuildArchive(data, '%s_archive_%s.tar.gz' %
                         (os.path.basename(args.exe_name), args.target_cpu))


if __name__ == '__main__':
  sys.exit(main())
