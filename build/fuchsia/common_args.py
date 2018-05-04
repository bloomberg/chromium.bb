# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from device_target import DeviceTarget
from qemu_target import QemuTarget


def AddCommonArgs(arg_parser):
  """Adds command line arguments to |arg_parser| for options which are shared
  across test and executable target types."""

  common_args = arg_parser.add_argument_group('common', 'Common arguments')
  common_args.add_argument('--package',
                           type=os.path.realpath, required=True,
                           help='Path to the package to execute.')
  common_args.add_argument('--package-name', required=True,
                           help='Name of the package to execute, defined in ' +
                                'package metadata.')
  common_args.add_argument('--package-manifest',
                           type=os.path.realpath, required=True,
                           help='Path to the Fuchsia package manifest file.')
  common_args.add_argument('--output-directory',
                           type=os.path.realpath, required=True,
                           help=('Path to the directory in which build files '
                                 'are located (must include build type).'))
  common_args.add_argument('--target-cpu', required=True,
                           help='GN target_cpu setting for the build.')
  common_args.add_argument('--device', '-d', action='store_true', default=False,
                           help='Run on hardware device instead of QEMU.')
  common_args.add_argument('--host', help='The IP of the target device. ' +
                           'Optional.')
  common_args.add_argument('--port', '-p', type=int, default=22,
                           help='The port of the SSH service running on the ' +
                                'device. Optional.')
  common_args.add_argument('--ssh-config', '-F',
                           help='The path to the SSH configuration used for '
                                'connecting to the target device.')
  common_args.add_argument('--include-system-logs', default=True, type=bool,
                           help='Do not show system log data.')
  common_args.add_argument('--verbose', '-v', default=False,
                           action='store_true',
                           help='Enable debug-level logging.')


def ConfigureLogging(args):
  """Configures the logging level based on command line |args|."""

  logging.basicConfig(level=(logging.DEBUG if args.verbose else logging.INFO),
                      format='%(asctime)s:%(levelname)s:%(name)s:%(message)s')

  # The test server spawner is too noisy with INFO level logging, so tweak
  # its verbosity a bit by adjusting its logging level.
  logging.getLogger('chrome_test_server_spawner').setLevel(
      logging.DEBUG if args.verbose else logging.WARN)

  # Verbose SCP output can be useful at times but oftentimes is just too noisy.
  # Only enable it if -vv is passed.
  logging.getLogger('ssh').setLevel(
      logging.DEBUG if args.verbose else logging.WARN)


def GetDeploymentTargetForArgs(args):
  """Constructs a deployment target object using parameters taken from
  command line arguments."""

  if not args.device:
    return QemuTarget(args.output_directory, args.target_cpu)
  else:
    return DeviceTarget(args.output_directory, args.target_cpu,
                        args.host, args.port, args.ssh_config)
