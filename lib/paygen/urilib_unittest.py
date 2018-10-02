# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the urilib module."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import gs
from chromite.lib import osutils

from chromite.lib.paygen import filelib
from chromite.lib.paygen import gslib
from chromite.lib.paygen import urilib


# We access private members to test them.
# pylint: disable=protected-access


class TestFileManipulation(cros_test_lib.TempDirTestCase):
  """Test general urilib file methods together."""

  # pylint: disable=attribute-defined-outside-init

  FILE1 = 'file1'
  FILE2 = 'file2'
  SUBDIR = 'subdir'
  SUBFILE = '%s/file3' % SUBDIR

  FILE1_CONTENTS = 'Howdy doody there dandy'
  FILE2_CONTENTS = 'Once upon a time in a galaxy far far away.'
  SUBFILE_CONTENTS = 'Five little monkeys jumped on the bed.'

  def setUp(self):
    # Use a subdir specifically for the cache so we can use the tempdir for
    # other things (including tempfiles by gsutil/etc...).
    self.filesdir = os.path.join(self.tempdir, 'unittest-cache')
    osutils.SafeMakedirs(self.filesdir)
    self.ctx = gs.GSContext()

  def _SetUpDirs(self, uri):
    self.gs_dir = uri

    self.file1_local = os.path.join(self.filesdir, self.FILE1)
    self.file2_local = os.path.join(self.filesdir, self.FILE2)
    self.subdir_local = os.path.join(self.filesdir, self.SUBDIR)
    self.subfile_local = os.path.join(self.filesdir, self.SUBFILE)

    self.file1_gs = os.path.join(self.gs_dir, self.FILE1)
    self.file2_gs = os.path.join(self.gs_dir, self.FILE2)
    self.subdir_gs = os.path.join(self.gs_dir, self.SUBDIR)
    self.subfile_gs = os.path.join(self.gs_dir, self.SUBFILE)

    # Pre-populate local dir with contents.
    with open(self.file1_local, 'w') as out1:
      out1.write(self.FILE1_CONTENTS)

    with open(self.file2_local, 'w') as out2:
      out2.write(self.FILE2_CONTENTS)

    os.makedirs(self.subdir_local)

    with open(self.subfile_local, 'w') as out3:
      out3.write(self.SUBFILE_CONTENTS)

  @cros_test_lib.NetworkTest()
  def testIntegration(self):
    with gs.TemporaryURL('urilib_unittest') as uri:
      self._runIntegrationTest(uri)

  def _runIntegrationTest(self, uri):
    """Run integration test under |uri|."""
    self._SetUpDirs(uri)

    self.assertTrue(self.ctx.Exists(self.filesdir, as_dir=True))
    self.assertTrue(self.ctx.Exists(self.file1_local))
    self.assertTrue(self.ctx.Exists(self.file2_local))
    self.assertTrue(self.ctx.Exists(self.subfile_local))
    self.assertTrue(self.ctx.Exists(self.subdir_local, as_dir=True))

    self.assertFalse(self.ctx.Exists(self.file1_gs))
    self.assertFalse(self.ctx.Exists(self.file2_gs))
    self.assertFalse(self.ctx.Exists(self.subfile_gs))

    shallow_local_files = [self.file1_local, self.file2_local]
    deep_local_files = shallow_local_files + [self.subfile_local]
    shallow_gs_files = [self.file1_gs, self.file2_gs]
    deep_gs_files = shallow_gs_files + [self.subfile_gs]

    # Test ListFiles, local version.
    self.assertEquals(set(shallow_local_files),
                      set(urilib.ListFiles(self.filesdir)))
    self.assertEquals(set(deep_local_files),
                      set(urilib.ListFiles(self.filesdir, recurse=True)))

    # Test CopyFiles, from local to GS.
    self.assertEquals(set(deep_gs_files),
                      set(urilib.CopyFiles(self.filesdir, self.gs_dir)))

    # Test ListFiles, GS version.
    self.assertEquals(set(shallow_gs_files),
                      set(urilib.ListFiles(self.gs_dir)))
    self.assertEquals(set(deep_gs_files),
                      set(urilib.ListFiles(self.gs_dir, recurse=True)))

    # Test Cmp between some files.
    self.assertTrue(urilib.Cmp(self.file1_local, self.file1_gs))
    self.assertFalse(urilib.Cmp(self.file2_local, self.file1_gs))

    # Test CopyFiles, from GS to local.
    self.assertEquals(set(deep_local_files),
                      set(urilib.CopyFiles(self.gs_dir, self.filesdir)))


class TestUrilib(cros_test_lib.MoxTempDirTestCase):
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
        self.assertEquals(protocol, urilib.ExtractProtocol(uri))

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
        self.assertEquals(uri_type, urilib.GetUriType(uri))

  def testMD5Sum(self):
    gs_path = 'gs://bucket/some/path'
    local_path = '/some/local/path'
    http_path = 'http://host.domain/some/path'

    self.mox.StubOutWithMock(gslib, 'MD5Sum')
    self.mox.StubOutWithMock(filelib, 'MD5Sum')

    # Set up the test replay script.
    # Run 1, GS.
    gslib.MD5Sum(gs_path).AndReturn('TheResult')
    # Run 3, local file.
    filelib.MD5Sum(local_path).AndReturn('TheResult')
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertEquals('TheResult', urilib.MD5Sum(gs_path))
    self.assertEquals('TheResult', urilib.MD5Sum(local_path))
    self.assertRaises(urilib.NotSupportedForType, urilib.MD5Sum, http_path)
    self.mox.VerifyAll()

  def testCmp(self):
    gs_path = 'gs://bucket/some/path'
    local_path = '/some/local/path'
    http_path = 'http://host.domain/some/path'

    result = 'TheResult'

    self.mox.StubOutWithMock(gslib, 'Cmp')
    self.mox.StubOutWithMock(filelib, 'Cmp')

    # Set up the test replay script.
    # Run 1, two local files.
    filelib.Cmp(local_path, local_path + '.1').AndReturn(result)
    # Run 2, local and GS.
    gslib.Cmp(local_path, gs_path).AndReturn(result)
    # Run 4, GS and GS
    gslib.Cmp(gs_path, gs_path + '.1').AndReturn(result)
    # Run 7, local and HTTP
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertEquals(result, urilib.Cmp(local_path, local_path + '.1'))
    self.assertEquals(result, urilib.Cmp(local_path, gs_path))
    self.assertEquals(result, urilib.Cmp(gs_path, gs_path + '.1'))
    self.assertRaises(urilib.NotSupportedBetweenTypes, urilib.Cmp,
                      local_path, http_path)
    self.mox.VerifyAll()

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

    result = 'TheResult'

    self.mox.StubOutWithMock(gslib, 'Copy')
    self.mox.StubOutWithMock(filelib, 'Copy')
    self.mox.StubOutWithMock(urilib, 'URLRetrieve')

    # Set up the test replay script.
    # Run 1, two local files.
    filelib.Copy(local_path, local_path + '.1').AndReturn(result)
    # Run 2, local and GS.
    gslib.Copy(local_path, gs_path).AndReturn(result)
    # Run 4, GS and GS
    gslib.Copy(gs_path, gs_path + '.1').AndReturn(result)
    # Run 7, HTTP and local
    urilib.URLRetrieve(http_path, local_path).AndReturn(result)
    # Run 8, local and HTTP
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertEquals(result, urilib.Copy(local_path, local_path + '.1'))
    self.assertEquals(result, urilib.Copy(local_path, gs_path))
    self.assertEquals(result, urilib.Copy(gs_path, gs_path + '.1'))
    self.assertEquals(result, urilib.Copy(http_path, local_path))
    self.assertRaises(urilib.NotSupportedBetweenTypes, urilib.Copy,
                      local_path, http_path)
    self.mox.VerifyAll()

  def testListFiles(self):
    gs_path = 'gs://bucket/some/path'
    local_path = '/some/local/path'
    http_path = 'http://host.domain/some/path'

    result = 'TheResult'
    patt = 'TheFilePattern'

    self.mox.StubOutWithMock(gslib, 'ListFiles')
    self.mox.StubOutWithMock(filelib, 'ListFiles')

    # Set up the test replay script.
    # Run 1, local.
    filelib.ListFiles(
        local_path, recurse=True, filepattern=None,
        sort=False).AndReturn(result)
    # Run 2, GS.
    gslib.ListFiles(
        gs_path, recurse=False, filepattern=patt, sort=True).AndReturn(result)
    # Run 4, HTTP.
    self.mox.ReplayAll()

    # Run the test verification.
    self.assertEquals(result, urilib.ListFiles(local_path, recurse=True))
    self.assertEquals(result, urilib.ListFiles(gs_path, filepattern=patt,
                                               sort=True))
    self.assertRaises(urilib.NotSupportedForType, urilib.ListFiles, http_path)
    self.mox.VerifyAll()
