# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for cipd."""

from __future__ import print_function

import hashlib
import json
import mock

import httplib2

from chromite.lib import cipd
from chromite.lib import cros_test_lib


class CIPDTest(cros_test_lib.MockTestCase):
  """Tests for chromite.lib.cipd"""

  def testDownloadCIPD(self):
    MockHttp = self.PatchObject(httplib2, 'Http')
    first_body = ")]}'\n" + json.dumps({
        'clientBinary': {
            'signedUrl': 'http://example.com',
        },
        'clientRefAliases': [
            {
                'hashAlgo': 'SKIP',
                'hexDigest': 'aaaa',
            },
            {
                'hashAlgo': 'SHA256',
                'hexDigest': 'bogus-sha256',
            },
        ],
    })
    response = mock.Mock()
    response.status = 200
    MockHttp.return_value.request.side_effect = [
        (response, first_body),
        (response, b'bogus binary file')]

    sha1 = self.PatchObject(hashlib, 'sha256')
    sha1.return_value.hexdigest.return_value = 'bogus-sha256'

    # Access to a protected member XXX of a client class
    # pylint: disable=protected-access
    self.assertTrue(cipd._DownloadCIPD('bogus-instance-sha256'))
