#!/usr/bin/env python
#
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages a user.bootfs for a Fuchsia boot image, pulling in the runtime
dependencies of a binary, and then uses either QEMU from the Fuchsia SDK
to run, or starts the bootserver to allow running on a hardware device."""

import argparse
import os
import sys

from runner_common import BuildBootfs, ImageCreationData, ReadRuntimeDeps, \
    RunFuchsia


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('--dry-run', '-n', action='store_true', default=False,
                      help='Just print commands, don\'t execute them.')
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
  parser.add_argument('-d', '--device', action='store_true', default=False,
                      help='Run on hardware device instead of QEMU.')
  parser.add_argument('--bootdata', type=os.path.realpath,
                      help='Path to a bootdata to use instead of the default '
                           'one from the SDK')
  parser.add_argument('--kernel', type=os.path.realpath,
                      help='Path to a kernel to use instead of the default '
                           'one from the SDK')
  parser.add_argument('--no-autorun', action='store_true',
                      help='Disable generating an autorun file')
  parser.add_argument('--extra-file', action='append', default=[],
                      help='Extra file to add to bootfs, '
                           '<bootfs_path>=<local_path>')
  args, child_args = parser.parse_known_args()

  runtime_deps = ReadRuntimeDeps(args.runtime_deps_path, args.output_directory)
  for extra_file in args.extra_file:
    parts = extra_file.split("=", 1)
    if len(parts) < 2:
      print 'Invalid --extra-file: ', extra_file
      print 'Expected format: --extra-file <bootfs_path>=<local_path>'
      return 2
    runtime_deps.append(tuple(parts))

  image_creation_data = ImageCreationData(
      output_directory=args.output_directory,
      exe_name=args.exe_name,
      runtime_deps=runtime_deps,
      target_cpu=args.target_cpu,
      dry_run=args.dry_run,
      child_args=child_args,
      use_device=args.device,
      bootdata=args.bootdata,
      wait_for_network=True,
      use_autorun=not args.no_autorun)
  bootfs = BuildBootfs(image_creation_data)
  if not bootfs:
    return 2

  return RunFuchsia(bootfs, args.device, args.kernel, args.dry_run, None)


if __name__ == '__main__':
  sys.exit(main())
