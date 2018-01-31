# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from device_target import DeviceTarget
from qemu_target import QemuTarget


def AddCommonArgs(arg_parser):
  """Adds command line arguments to |arg_parser| for options which are shared
  across test and executable target types."""

  common_args = arg_parser.add_argument_group('common', 'Common arguments')
  common_args.add_argument('--package',
                           type=os.path.realpath, required=True,
                           help='Path to the package to execute.')
  common_args.add_argument('--package-manifest',
                           type=os.path.realpath, required=True,
                           help='Path to the Fuchsia package manifest file.')
  common_args.add_argument('--output-directory',
                           type=os.path.realpath, required=True,
                           help=('Path to the directory in which build files are'
                                 ' located (must include build type).'))
  common_args.add_argument('--target-cpu', required=True,
                           help='GN target_cpu setting for the build.')
  common_args.add_argument('--device', '-d', action='store_true', default=False,
                           help='Run on hardware device instead of QEMU.')
  common_args.add_argument('--host', help='The IP of the target device. ' +
                           'Optional.')
  common_args.add_argument('--port', '-p', type=int, default=22,
                           help='The port of the SSH service running on the ' +
                                'device. Optional.')
  common_args.add_argument('--ssh_config', '-F',
                           help='The path to the SSH configuration used for '
                                'connecting to the target device.')
  common_args.add_argument('--verbose', '-v', default=False, action='store_true',
                           help='Show more logging information.')


def ConfigureLogging(args):
  """Configures the logging level based on command line |args|."""

  logging.basicConfig(level=(logging.DEBUG if args.verbose else logging.INFO))


def GetDeploymentTargetForArgs(args):
  """Constructs a deployment target object using parameters taken from
  command line arguments."""

  if not args.device:
    return QemuTarget(args.output_directory, args.target_cpu)
  else:
    return DeviceTarget(args.output_directory, args.target_cpu,
                        args.host, args.port, args.ssh_config)
