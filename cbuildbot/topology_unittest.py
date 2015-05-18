# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for topology module."""

from __future__ import print_function

from chromite.cbuildbot import topology
from chromite.lib import fake_cidb
from chromite.lib import cros_test_lib


class ToplogyTest(cros_test_lib.TestCase):
  """Unit test of topology module."""

  def setUp(self):
    # Mutually isolate these tests and make them independent of
    # TOPOLOGY_DEFAULTS
    topology.topology = topology.LockedDefaultDict()

  def testWithDB(self):
    fake_db = fake_cidb.FakeCIDBConnection(fake_keyvals={'/foo': 'bar'})
    topology.FetchTopologyFromCIDB(fake_db)
    self.assertEqual(topology.topology.get('/foo'), 'bar')

  def testWithoutDB(self):
    topology.FetchTopologyFromCIDB(None)
    self.assertEqual(topology.topology.get('/foo'), None)

  def testNotFetched(self):
    with self.assertRaises(topology.LockedDictAccessException):
      topology.topology.get('/foo')
