# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros payload: Show information about an update payload."""

from __future__ import print_function

import itertools
import os
import sys
import textwrap

from chromite.cbuildbot import constants
from chromite.cli import command

# Needed for the dev.host.lib import below.
sys.path.insert(0, os.path.join(constants.SOURCE_ROOT, 'src', 'platform'))


def DisplayValue(key, value):
  """Print out a key, value pair with values left-aligned."""
  if value != None:
    print('%-*s %s' % (24, key + ':', value))
  else:
    raise ValueError('Cannot display an empty value.')


@command.CommandDecorator('payload')
class PayloadCommand(command.CliCommand):
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

    super(PayloadCommand, self).__init__(options)
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

    The list shown includes operation type, data offset, data length, source
    extents, source length, destination extents, and destinations length.

    Args:
      name: The name you want displayed above the operation table.
      operations: The install_operations object that you want to display
                  information about.
    """
    def _DisplayExtents(extents, name):
      num_blocks = sum([ext.num_blocks for ext in extents])
      ext_str = ' '.join(
          '(%s,%s)' % (ext.start_block, ext.num_blocks) for ext in extents)
      # Make extent list wrap around at 80 chars.
      ext_str = '\n      '.join(textwrap.wrap(ext_str, 74))
      extent_plural = 's' if len(extents) > 1 else ''
      block_plural = 's' if num_blocks > 1 else ''
      print('    %s: %d extent%s (%d block%s)' %
            (name, len(extents), extent_plural, num_blocks, block_plural))
      print('      %s' % ext_str)

    op_dict = self._update_payload.common.OpType.NAMES
    print('%s:' % name)
    for op, op_count in itertools.izip(operations, itertools.count()):
      print('  %d: %s' % (op_count, op_dict[op.type]))
      if op.HasField('data_offset'):
        print('    Data offset: %s' % op.data_offset)
      if op.HasField('data_length'):
        print('    Data length: %s' % op.data_length)
      if len(op.src_extents):
        _DisplayExtents(op.src_extents, 'Source')
      if len(op.dst_extents):
        _DisplayExtents(op.dst_extents, 'Destination')

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
