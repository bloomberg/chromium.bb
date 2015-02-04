# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros payload: Show information about an update payload."""

from __future__ import print_function

import os
import sys

from chromite import cros
from chromite.cbuildbot import constants

# Needed for the dev.host.lib import below.
sys.path.insert(0, os.path.join(constants.SOURCE_ROOT, 'src', 'platform'))

# TODO(alliewood)(chromium:454629) update once update_payload is moved
# into chromite
from dev.host.lib import update_payload


def DisplayValue(key, value):
  """Print out a key, value pair with values left-aligned."""
  if value != None:
    print('{:<24} {}'.format(key+':', value))
  else:
    raise ValueError("Cannot display an empty value.")


@cros.CommandDecorator('payload')
class PayloadCommand(cros.CrosCommand):
  """Show basic information about an update payload.

  This command parses an update payload and displays information from
  its header and manifest.
  """

  EPILOG = """
Example:
  cros payload show chromeos_6716.0.0_daisy_canary-channel_full_snow-mp-v3.bin
"""

  def __init__(self, options):
    cros.CrosCommand.__init__(self, options)
    self.payload = None

  @classmethod
  def AddParser(cls, parser):
    super(PayloadCommand, cls).AddParser(parser)
    parser.add_argument(
        "action", choices=['show'],
        help="Show information about an update payload.")
    parser.add_argument(
        "payload_file", type=file,
        help="The payload file that you want information from.")

  def _DisplayHeader(self):
    """Show information from the payload header."""
    header = self.payload.header
    DisplayValue('Payload version', header.version)
    DisplayValue('Manifest length', header.manifest_len)

  def _DisplayManifest(self):
    """Show information from the payload manifest."""
    manifest = self.payload.manifest
    DisplayValue('Number of operations', len(manifest.install_operations))
    DisplayValue('Number of kernel ops',
                 len(manifest.kernel_install_operations))
    DisplayValue('Block size', manifest.block_size)

  def Run(self):
    """Parse the update payload and display information from it."""
    self.payload = update_payload.Payload(self.options.payload_file)
    self.payload.Init()
    self._DisplayHeader()
    self._DisplayManifest()

