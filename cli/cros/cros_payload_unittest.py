# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros payload command."""

from __future__ import print_function

import collections
import os
import sys

from chromite.cbuildbot import constants
from chromite.cli.cros import cros_payload
from chromite.lib import cros_test_lib

# Needed for the dev.host.lib import below.
sys.path.insert(0, os.path.join(constants.SOURCE_ROOT, 'src', 'platform'))

# TODO(alliewood)(chromium:454629) update once update_payload is moved
# into chromite
from dev.host.lib import update_payload

class FakeOption(object):
  """Fake options object for testing."""

  def __init__(self, **kwargs):
    self.list_ops = False
    self.stats = False
    for key, val in kwargs.iteritems():
      setattr(self, key, val)
    if not hasattr(self, 'payload_file'):
      self.payload_file = None

class FakeOp(object):
  """Fake manifest operation for testing."""

  def __init__(self, src_extents, dst_extents, op_type, **kwargs):
    self.src_extents = src_extents
    self.dst_extents = dst_extents
    self.type = op_type
    for key, val in kwargs.iteritems():
      setattr(self, key, val)

  def HasField(self, field):
    return hasattr(self, field)

class FakeManifest(object):
  """Fake manifest for testing."""

  def __init__(self):
    FakeExtent = collections.namedtuple('FakeExtent',
                                        ['start_block', 'num_blocks'])
    self.install_operations = [FakeOp([],
                                      [FakeExtent(1, 1), FakeExtent(2, 2)],
                                      update_payload.common.OpType.REPLACE_BZ,
                                      dst_length=3*4096,
                                      data_offset=1,
                                      data_length=1)]
    self.kernel_install_operations = [FakeOp(
        [FakeExtent(1, 1)],
        [FakeExtent(x, x) for x in xrange(20)],
        update_payload.common.OpType.SOURCE_COPY,
        src_length=4096)]
    self.block_size = 4096
    self.minor_version = 4
    FakePartInfo = collections.namedtuple('FakePartInfo', ['size'])
    self.old_rootfs_info = FakePartInfo(1 * 4096)
    self.old_kernel_info = FakePartInfo(2 * 4096)
    self.new_rootfs_info = FakePartInfo(3 * 4096)
    self.new_kernel_info = FakePartInfo(4 * 4096)

class FakePayload(object):
  """Fake payload for testing."""

  def __init__(self):
    self.header = None
    self.manifest = None

  def Init(self):
    """Fake Init that sets header and manifest."""
    FakeHeader = collections.namedtuple('FakeHeader',
                                        ['version', 'manifest_len'])
    self.header = FakeHeader('111', 222)
    self.manifest = FakeManifest()

class PayloadCommandTest(cros_test_lib.MockOutputTestCase):
  """Test class for our PayloadCommand class."""

  def testDisplayValue(self):
    """Verify that DisplayValue prints what we expect."""
    with self.OutputCapturer() as output:
      cros_payload.DisplayValue('key', 'value')
    stdout = output.GetStdout()
    self.assertEquals(stdout, 'key:                     value\n')

  def testRun(self):
    """Verify that Run parses and displays the payload like we expect."""
    payload_cmd = cros_payload.PayloadCommand(FakeOption(action='show'))
    self.PatchObject(update_payload, 'Payload', return_value=FakePayload())

    with self.OutputCapturer() as output:
      payload_cmd.Run()

    stdout = output.GetStdout()
    expected_out = """Payload version:         111
Manifest length:         222
Number of operations:    1
Number of kernel ops:    1
Block size:              4096
Minor version:           4
"""
    self.assertEquals(stdout, expected_out)

  def testListOps(self):
    """Verify that the --list_ops option gives the correct output."""
    payload_cmd = cros_payload.PayloadCommand(FakeOption(list_ops=True,
                                                         action='show'))
    self.PatchObject(update_payload, 'Payload', return_value=FakePayload())

    with self.OutputCapturer() as output:
      payload_cmd.Run()

    stdout = output.GetStdout()
    expected_out = """Payload version:         111
Manifest length:         222
Number of operations:    1
Number of kernel ops:    1
Block size:              4096
Minor version:           4

Install operations:
  0: REPLACE_BZ
    Data offset: 1
    Data length: 1
    Destination: 2 extents (3 blocks)
      (1,1) (2,2)
Kernel install operations:
  0: SOURCE_COPY
    Source: 1 extent (1 block)
      (1,1)
    Destination: 20 extents (190 blocks)
      (0,0) (1,1) (2,2) (3,3) (4,4) (5,5) (6,6) (7,7) (8,8) (9,9) (10,10)
      (11,11) (12,12) (13,13) (14,14) (15,15) (16,16) (17,17) (18,18) (19,19)
"""
    self.assertEquals(stdout, expected_out)

  def testStats(self):
    """Verify that the --stats option works correctly."""
    payload_cmd = cros_payload.PayloadCommand(FakeOption(stats=True,
                                                         action='show'))
    self.PatchObject(update_payload, 'Payload', return_value=FakePayload())

    with self.OutputCapturer() as output:
      payload_cmd.Run()

    stdout = output.GetStdout()
    expected_out = """Payload version:         111
Manifest length:         222
Number of operations:    1
Number of kernel ops:    1
Block size:              4096
Minor version:           4
Blocks read:             11
Blocks written:          193
Seeks when writing:      18
"""
    self.assertEquals(stdout, expected_out)
