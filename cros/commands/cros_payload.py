# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros payload: Show information about an update payload."""

from __future__ import print_function

import os
import sys

from chromite import cros
from chromite.cbuildbot import constants
from chromite.lib import table

# Needed for the dev.host.lib import below.
sys.path.insert(0, os.path.join(constants.SOURCE_ROOT, 'src', 'platform'))


def DisplayValue(key, value):
  """Print out a key, value pair with values left-aligned."""
  if value != None:
    print('%-*s %s' % (24, key + ':', value))
  else:
    raise ValueError('Cannot display an empty value.')


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
    # TODO(alliewood)(chromium:454629) update once update_payload is moved
    # into chromite. google.protobuf may not be available outside the chroot.
    from dev.host.lib import update_payload
    self._update_payload = update_payload

    cros.CrosCommand.__init__(self, options)
    self.payload = None

  @classmethod
  def AddParser(cls, parser):
    super(PayloadCommand, cls).AddParser(parser)
    parser.add_argument(
        'action', choices=['show'],
        help='Show information about an update payload.')
    parser.add_argument(
        'payload_file', type=file,
        help='The payload file that you want information from.')
    parser.add_argument('--list_ops', default=False, action='store_true',
                        help='List the install operations and their extents.')

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
    DisplayValue('Minor version', manifest.minor_version)

  def _DisplayOps(self, name, operations):
    """Show information about the install operations from the manifest.

    The table shown includes operation type, data offset, data length, source
    extents, and destinations extents.

    Args:
      name: The name you want displayed above the operation table.
      operations: The install_operations object that you want to display
                  information about.
    """
    optable = table.Table(['Op Type', 'Offset', 'Data len', 'Src exts',
                           'Dst exts'])
    op_dict = self._update_payload.common.OpType.NAMES
    for op in operations:
      src_extents = ''.join('(%s, %s)' % (extent.start_block, extent.num_blocks)
                            for extent in op.src_extents)
      dst_extents = ''.join('(%s, %s)' % (extent.start_block, extent.num_blocks)
                            for extent in op.dst_extents)
      if not src_extents:
        src_extents = '()'
      if not dst_extents:
        dst_extents = '()'
      optable.AppendRow([op_dict[op.type], op.data_offset, op.data_length,
                         src_extents, dst_extents])
    print('%s:\n%s' % (name, optable))

  def Run(self):
    """Parse the update payload and display information from it."""
    self.payload = self._update_payload.Payload(self.options.payload_file)
    self.payload.Init()
    self._DisplayHeader()
    self._DisplayManifest()
    if self.options.list_ops:
      print()
      self._DisplayOps('Install operations',
                       self.payload.manifest.install_operations)
      self._DisplayOps('Kernel install operations',
                       self.payload.manifest.kernel_install_operations)
