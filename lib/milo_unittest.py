# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for milo."""

from __future__ import print_function

import mock
import os

from chromite.lib import auth
from chromite.lib import cros_test_lib
from chromite.lib import milo


TESTDATA_PATH = os.path.join(os.path.dirname(__file__), 'testdata', 'milo')


class MiloClientTest(cros_test_lib.MockTestCase):
  """Tests for MiloClient."""
  def setUp(self):
    self.mock_request = self.PatchObject(milo.MiloClient, 'SendRequest')
    self.PatchObject(milo.MiloClient, '_GetHost', return_value='foo.com')
    self.PatchObject(auth, 'GetAccessToken', return_value='token')
    self.client = milo.MiloClient()

  def testGetBuildbotBuildJSON(self):
    """Test GetBuildbotBuildJSON."""
    # Datagram corresponding to: chromeos elm-paladin 1535
    with open(os.path.join(
        TESTDATA_PATH, 'MiloClientTest.testGetBuildbotBuildJSON.data')) as f:
      datagram = f.read()
    self.mock_request.return_value = {
        'data': datagram,
    }
    resp = self.client.GetBuildbotBuildJSON('chromeos', 'elm-paladin', 1535)
    self.assertEqual(resp['slave'], 'build115-m2')
    self.assertEqual(resp['properties']['buildername'], 'elm-paladin')
    self.assertEqual(resp['steps']['BuildPackages']['logs'][0][1],
                     ('https://uberchromegw.corp.google.com/i/chromeos/'
                      'builders/elm-paladin/builds/1535/steps/BuildPackages/'
                      'logs/stdio'))
    self.assertEqual(resp['masterName'], 'chromeos')
    self.mock_request.assert_called_with('prpc/milo.Buildbot',
                                         'GetBuildbotBuildJSON', mock.ANY,
                                         dryrun=False)
