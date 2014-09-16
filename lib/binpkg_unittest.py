#!/usr/bin/python
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the binpkg.py module."""

from __future__ import print_function

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import binpkg
from chromite.lib import cros_test_lib
from chromite.lib import gs_unittest


class FetchTarballsTest(cros_test_lib.MockTempDirTestCase):
  """Tests for GSContext that go over the network."""

  def testFetchFakePackages(self):
    """Pretend to fetch binary packages."""
    gs_mock = self.StartPatcher(gs_unittest.GSContextMock())
    gs_mock.SetDefaultCmdResult()
    uri = 'gs://foo/bar'
    packages_uri = '{}/Packages'.format(uri)
    packages_file = '''URI: gs://foo

CPV: boo/baz
PATH boo/baz.tbz2
'''
    gs_mock.AddCmdResult(['cat', packages_uri], output=packages_file)

    binpkg.FetchTarballs([uri], self.tempdir)

  @cros_test_lib.NetworkTest()
  def testFetchRealPackages(self):
    """Actually fetch a real binhost from the network."""
    uri = 'gs://chromeos-prebuilt/board/lumpy/paladin-R37-5905.0.0-rc2/packages'
    binpkg.FetchTarballs([uri], self.tempdir)


if __name__ == '__main__':
  cros_test_lib.main()
