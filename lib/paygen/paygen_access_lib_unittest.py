#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the paygen_access_lib module."""

import mox
import time
import unittest

import fixup_path
fixup_path.FixupPath()

from chromite.lib.paygen import paygen_access_lib


class FakeGslibError(Exception):
  """FakeGslib base exception class."""


class FakeGslibNoSuchBucket(FakeGslibError):
  """FakeGslib error due to non-existent bucket."""


class FakeGslibNoSuchObject(FakeGslibError):
  """FakeGslib error due to non-existent object."""


class FakeGslibObjectMismatch(FakeGslibError):
  """FakeGslib error due to mismatched object attributes."""


class FakeGsFile(object):
  """A fake GS file."""

  def __init__(self, content, generation):
    self.content = content
    self.generation = generation


class FakeGslib(object):
  """A lightweight and partial fake implementation of gslib."""

  class CopyFail(FakeGslibError):
    """Error during Copy (mirroring a gslib exception)."""

  class CatFail(FakeGslibError):
    """Error during Cat (mirroring a gslib exception)."""

  def __init__(self, *bucket_names):
    self.bucket_names = bucket_names
    self.store = {}
    self.do_fail_copy = False
    self.do_fail_cat = False

  def _ValidateBucket(self, uri):
    for bucket in self.bucket_names:
      if uri.startswith('gs://%s/' % bucket):
        return True

    raise FakeGslibNoSuchBucket('No bucket found for %s' % uri)

  def _CheckGsUri(self, uri):
    return uri.startswith('gs://') and self._ValidateBucket(uri)

  def Copy(self, src_path, dst_path, generation=None):
    try:
      if not self._CheckGsUri(dst_path):
        return  # Nothing to do.

      if self.do_fail_copy:
        raise FakeGslibError('Forced failure')

      if self._CheckGsUri(src_path):
        src_file = self.store.get(src_path)
        if not src_file:
          raise FakeGslibNoSuchObject('Source file %s not found' % src_path)
        content = src_file.content
      else:
        with open(src_path) as src_file:
          content = src_file.read()

      if generation is None:
        generation = 0
      else:
        curr_dst_file = self.store.get(dst_path)
        if generation:
          if not curr_dst_file:
            raise FakeGslibNoSuchObject('File %s does not exist' % dst_path)
          elif generation != curr_dst_file.generation:
            raise FakeGslibObjectMismatch(
                'File %s already exists with different generation' % dst_path)
        elif curr_dst_file:
          raise self.CopyFail('File %s already exists' % dst_path)

      self.store[dst_path] = FakeGsFile(content, generation)

    except Exception as e:
      raise self.CopyFail(str(e))

  def Exists(self, uri):
    try:
      return self._CheckGsUri(uri) and bool(self.store.get(uri))
    except FakeGslibNoSuchBucket:
      return False

  def Cat(self, uri):
    try:
      self._CheckGsUri(uri)
      if self.do_fail_cat:
        raise FakeGslibError('Forced failure')

      gs_file = self.store.get(uri)
      if not gs_file:
        raise FakeGslibNoSuchObject('File %s does not exist' % uri)
      return gs_file.content
    except FakeGslibError as e:
      raise self.CatFail('Error catting file %s: %s' % (uri, e))


class PaygenAccessLibTest(mox.MoxTestBase):
  """Testing the paygen_access_lib functions."""

  def __init__(self, test_case_names):
    mox.MoxTestBase.__init__(self, test_case_names)

    self.test_channel = 'foo-channel'
    self.test_board = 'foo-board'
    self.test_version = 'foo-version'
    self.test_build = (self.test_channel, self.test_board, self.test_version)

  def setUp(self):
    self.mox = mox.Mox()

    self.orig_gslib = paygen_access_lib.gslib
    paygen_access_lib.gslib = self.fake_gslib = FakeGslib('chromeos-releases')

  def tearDown(self):
    self.mox.UnsetStubs()
    paygen_access_lib.gslib = self.orig_gslib

  def testFirstUpdateAndGetMarkers(self):
    paygen_access_lib.UpdateMarkers(*self.test_build)
    first_timestamp, last_timestamp = paygen_access_lib.GetMarkers(
        *self.test_build)
    self.assertEqual(first_timestamp, last_timestamp)

  def testTwoUpdatesAndGetMarkers(self):
    paygen_access_lib.UpdateMarkers(*self.test_build)
    time.sleep(1)
    paygen_access_lib.UpdateMarkers(*self.test_build)
    first_timestamp, last_timestamp = paygen_access_lib.GetMarkers(
        *self.test_build)
    self.assertLess(first_timestamp, last_timestamp)

  def testGetMarkersWithoutUpdate(self):
    self.assertRaises(paygen_access_lib.MarkerReadError,
                      paygen_access_lib.GetMarkers, *self.test_build)

  def testFailUpdateMarkers(self):
    self.fake_gslib.do_fail_copy = True
    self.assertRaises(paygen_access_lib.MarkerWriteError,
                      paygen_access_lib.UpdateMarkers, *self.test_build)

  def testFailGetMarkers(self):
    paygen_access_lib.UpdateMarkers(*self.test_build)
    self.fake_gslib.do_fail_cat = True
    self.assertRaises(paygen_access_lib.MarkerReadError,
                      paygen_access_lib.GetMarkers, *self.test_build)


if __name__ == '__main__':
  unittest.main()
