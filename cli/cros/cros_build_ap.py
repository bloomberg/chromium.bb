# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Build the AP firmware for a build target."""

from __future__ import print_function

from chromite.cli import command
from chromite.lib import build_target_util
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib.firmware import ap_firmware


@command.CommandDecorator('build-ap')
class BuildApCommand(command.CliCommand):
  """Build the AP Firmware for the requested build target."""

  EPILOG = """
  To build the AP Firmware for foo:
    cros build-ap -b foo
  """

  def __init__(self, options):
    super(BuildApCommand, self).__init__(options)
    target_name = self.options.build_target or cros_build_lib.GetDefaultBoard()
    self.build_target = build_target_util.BuildTarget(target_name)

  @classmethod
  def AddParser(cls, parser):
    super(cls, BuildApCommand).AddParser(parser)

    parser.add_argument(
        '-b',
        '--board',
        '--build-target',
        dest='build_target',
        help='The build target whose AP firmware should be built.'
    )

  def Run(self):
    """Run cros build."""
    self.options.Freeze()

    commandline.RunInsideChroot(self)

    try:
      ap_firmware.build(self.build_target)
    except ap_firmware.Error as e:
      cros_build_lib.Die(e)
