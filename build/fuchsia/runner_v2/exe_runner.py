#!/usr/bin/env python
#
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Packages a user.bootfs for a Fuchsia boot image, pulling in the runtime
dependencies of a test binary, and then uses either QEMU from the Fuchsia SDK
to run, or starts the bootserver to allow running on a hardware device."""

import argparse
import logging
import sys

from common_args import AddCommonArgs, ConfigureLogging, \
                        GetDeploymentTargetForArgs
from run_package import RunPackage


def main():
  parser = argparse.ArgumentParser()
  AddCommonArgs(parser)
  parser.add_argument('child_args', nargs='*',
                      help='Arguments for the test process.')
  args = parser.parse_args()
  ConfigureLogging(args)

  with GetDeploymentTargetForArgs(args) as target:
    target.Start()
    RunPackage(args.output_directory, target, args.package,
               args.package_name, args.child_args, args.package_manifest)


if __name__ == '__main__':
  sys.exit(main())
