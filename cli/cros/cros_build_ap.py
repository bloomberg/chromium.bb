# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build the AP firmware for a build target."""

from __future__ import print_function

import sys

from chromite.cli import command
from chromite.lib import build_target_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib.firmware import ap_firmware

assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


@command.CommandDecorator('build-ap')
class BuildApCommand(command.CliCommand):
  """Build the AP Firmware for the requested build target."""

  EPILOG = """
To build the AP Firmware for foo:
  cros build-ap -b foo

To build the AP Firmware only for foo-variant:
  cros build-ap -b foo --fw-name foo-variant
"""

  def __init__(self, options):
    super(BuildApCommand, self).__init__(options)
    self.build_target = build_target_lib.BuildTarget(self.options.build_target)

  @classmethod
  def AddParser(cls, parser):
    super(cls, BuildApCommand).AddParser(parser)

    parser.add_argument(
        '-b',
        '--build-target',
        dest='build_target',
        default=cros_build_lib.GetDefaultBoard(),
        required=not bool(cros_build_lib.GetDefaultBoard()),
        help='The build target whose AP firmware should be built.')

    parser.add_argument(
        '--fw-name',
        '--variant',
        dest='fw_name',
        help='Sets the FW_NAME environment variable. Set to build only the '
             "specified variant's firmware.")

    # TODO(saklein): Remove when added to base parser.
    parser.add_argument(
        '-n',
        '--dry-run',
        action='store_true',
        default=False,
        help='Perform a dry run, describing the steps without running them.')
    parser.add_argument(
        '-v',
        '--verbose',
        action='store_true',
        default=False,
        help='More verbose output than the default, but less than debug. An '
             'alias for --log-level=info.')

  def Run(self):
    """Run cros build-ap."""
    self.options.Freeze()

    commandline.RunInsideChroot(self)

    try:
      ap_firmware.build(
          self.build_target,
          fw_name=self.options.fw_name,
          dry_run=self.options.dry_run)
    except ap_firmware.Error as e:
      cros_build_lib.Die(e)
