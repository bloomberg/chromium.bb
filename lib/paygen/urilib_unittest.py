# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the urilib module."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import gs

from chromite.lib.paygen import filelib
from chromite.lib.paygen import urilib


# We access private members to test them.
# pylint: disable=protected-access


class TestUrilib(cros_test_lib.MockTempDirTestCase):
  """Test urilib module."""

  def testExtractProtocol(self):
    tests = {'gs': ['gs://',
                    'gs://foo',
                    'gs://foo/bar'],
             'abc': ['abc://',
                     'abc://foo',
                     'abc://foo/bar'],
             None: ['foo/bar',
                    '/foo/bar',
                    '://garbage/path']}

    for protocol in tests:
      for uri in tests[protocol]:
        self.assertEqual(protocol, urilib.ExtractProtocol(uri))

  def testGetUriType(self):
    tests = {'gs': ['gs://',
                    'gs://foo',
                    'gs://foo/bar'],
             'abc': ['abc://',
                     'abc://foo',
                     'abc://foo/bar'],
             'file': ['foo/bar',
                      '/foo/bar',
                      '://garbage/path',
                      '/cnsfoo/bar']}

    for uri_type in tests:
      for uri in tests[uri_type]:
        self.assertEqual(uri_type, urilib.GetUriType(uri))

  @cros_test_lib.NetworkTest()
  def testURLRetrieve(self):
    good_url = 'https://codereview.chromium.org/download/issue11731004_1_2.diff'
    bad_domain_url = 'http://notarealdomainireallyhope.com/some/path'
    bad_path_url = 'https://dl.google.com/dl/edgedl/x/y/z/a/b/c/foobar'
    local_path = os.path.join(self.tempdir, 'downloaded_file')
    bad_local_path = '/tmp/a/b/c/d/x/y/z/foobar'

    git_index1 = 'e6c0d72a5122171deb4c458991d1c7547f31a2f0'
    git_index2 = '3d0f7d3edfd8146031e66dc3f45926920d3ded78'
    expected_contents = """Index: LICENSE
diff --git a/LICENSE b/LICENSE
index %s..%s 100644
--- a/LICENSE
+++ b/LICENSE
@@ -1,4 +1,4 @@
-// Copyright (c) 2012 The Chromium Authors. All rights reserved.
+// Copyright (c) 2013 The Chromium Authors. All rights reserved.
 //
 // Redistribution and use in source and binary forms, with or without
 // modification, are permitted provided that the following conditions are
""" % (git_index1, git_index2)

    self.assertRaises(urilib.MissingURLError, urilib.URLRetrieve,
                      bad_path_url, local_path)
    self.assertRaises(urilib.MissingURLError, urilib.URLRetrieve,
                      bad_domain_url, local_path)

    urilib.URLRetrieve(good_url, local_path)
    with open(local_path, 'r') as f:
      actual_contents = f.read()
    self.assertEqual(expected_contents, actual_contents)

    self.assertRaises(IOError, urilib.URLRetrieve, good_url, bad_local_path)

  def testCopy(self):
    gs_path = 'gs://bucket/some/path'
    local_path = '/some/local/path'
    http_path = 'http://host.domain/some/path'

    gs_mock = self.PatchObject(gs.GSContext, 'Copy')
    file_mock = self.PatchObject(filelib, 'Copy')
    urlretrieve_mock = self.PatchObject(urilib, 'URLRetrieve')

    # Set up the test replay script.
    # Run 1, two local files.
    urilib.Copy(local_path, local_path + '.1')
    file_mock.assert_called_once_with(local_path, local_path + '.1')
    file_mock.reset_mock()

    # Run 2, local and GS.
    urilib.Copy(local_path, gs_path)
    gs_mock.assert_called_once_with(local_path, gs_path)
    gs_mock.reset_mock()

    # Run 4, GS and GS
    urilib.Copy(gs_path, gs_path + '.1')
    gs_mock.assert_called_once_with(gs_path, gs_path + '.1')
    gs_mock.reset_mock()

    # Run 7, HTTP and local
    urilib.Copy(http_path, local_path)
    urlretrieve_mock.assert_called_once_with(http_path, local_path)
    urlretrieve_mock.reset_mock()

    # Run 8, local and HTTP
    self.assertRaises(urilib.NotSupportedBetweenTypes, urilib.Copy,
                      local_path, http_path)

  def testGetPathExcludingProtocol(self):
    """Tests GetPathExcludingProtocol."""
    self.assertEqual(urilib.GetPathExcludingProtocol('foo-file'), 'foo-file')
    self.assertEqual(urilib.GetPathExcludingProtocol('foo/directory/foo-file'),
                     'foo/directory/foo-file')
    self.assertEqual(urilib.GetPathExcludingProtocol('/foo-file'), '/foo-file')
    self.assertEqual(urilib.GetPathExcludingProtocol('/foo/directory/foo-file'),
                     '/foo/directory/foo-file')
    self.assertEqual(urilib.GetPathExcludingProtocol('file:///foo-file'),
                     '/foo-file')
    self.assertEqual(urilib.GetPathExcludingProtocol('gs://foo-file'),
                     'foo-file')
    self.assertEqual(urilib.GetPathExcludingProtocol('http://foo-file'),
                     'foo-file')
    self.assertEqual(urilib.GetPathExcludingProtocol('https://foo-file'),
                     'foo-file')
    self.assertEqual(
        urilib.GetPathExcludingProtocol('https://foo-directory/foo-file'),
        'foo-directory/foo-file')
