# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This module tests the cros payload command."""

from __future__ import print_function

import collections
import os
import sys

from chromite.cbuildbot import constants
from chromite.cros.commands import cros_payload
from chromite.lib import cros_test_lib

# Needed for the dev.host.lib import below.
sys.path.insert(0, os.path.join(constants.SOURCE_ROOT, 'src', 'platform'))

# TODO(alliewood)(chromium:454629) update once update_payload is moved
# into chromite
from dev.host.lib import update_payload


class FakePayload(object):
  """Fake payload for testing."""

  def __init__(self):
    self.header = None
    self.manifest = None

  def Init(self):
    """Fake Init that sets header and manifest."""
    FakeHeader = collections.namedtuple('FakeHeader',
                                        ['version', 'manifest_len'])
    FakeManifest = collections.namedtuple('FakeManifest',
                                          ['install_operations',
                                           'kernel_install_operations',
                                           'block_size'])
    self.header = FakeHeader('111', '222')
    self.manifest = FakeManifest([1, 2, 3], [0], '333')


class PayloadCommandTest(cros_test_lib.MockOutputTestCase):
  """Test class for our PayloadCommand class."""

  def testDisplayValue(self):
    """Verify that DisplayValue prints what we expect."""
    with self.OutputCapturer() as output:
      cros_payload.DisplayValue('key', 'value')
    stdout = output.GetStdout()
    self.assertEquals(stdout, "key:                     value\n")

  def testRun(self):
    """Verify that Run parses and displays the payload like we expect."""
    FakeOption = collections.namedtuple('FakeOption', ['payload_file'])
    payload_cmd = cros_payload.PayloadCommand(FakeOption(None))
    self.PatchObject(update_payload, 'Payload', return_value=FakePayload())

    with self.OutputCapturer() as output:
      payload_cmd.Run()

    stdout = output.GetStdout()
    expected_out = """Payload version:         111
Manifest length:         222
Number of operations:    3
Number of kernel ops:    1
Block size:              333
"""
    self.assertEquals(stdout, expected_out)
