# -*- coding: utf-8 -*-
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the filelib module."""

from __future__ import print_function

import base64
import os

from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib.paygen import filelib


class TestFileLib(cros_test_lib.TempDirTestCase):
  """Test filelib module.

  Note: We use tools for hashes to avoid relying on hashlib since that's what
  the filelib module uses.  We want to verify things rather than have a single
  hashlib bug break both the code and the tests.
  """

  @staticmethod
  def _ShaSum(util, file_path):
    """Use external shasum utility to get hash of a file.

    We use the external utility rather than hashlib as our filelib module is
    based on hashlib, and we'd like to have an independent implementation to
    compare against (rather than the two modules be bug-for-bug compatible).
    """
    # The shasum utility returns hashes in base 16 encoding.  We need base 64.
    hash16 = cros_build_lib.RunCommand(
        [util, file_path], redirect_stdout=True).output.split(' ')[0]
    hashbytes = base64.b16decode(hash16, True)
    hash64 = base64.b64encode(hashbytes)
    return hash64.rstrip().decode('utf-8')

  def testShaSums(self):
    file_path = os.path.abspath(__file__)
    expected_sha1 = self._ShaSum('sha1sum', file_path)
    expected_sha256 = self._ShaSum('sha256sum', file_path)
    sha1, sha256 = filelib.ShaSums(file_path)
    self.assertEqual(expected_sha1, sha1)
    self.assertEqual(expected_sha256, sha256)

  def testCopyIntoExistingDir(self):
    """Copy a file into a dir that exists."""
    path1 = os.path.join(self.tempdir, 'path1')
    subdir = os.path.join(self.tempdir, 'subdir')
    path2 = os.path.join(subdir, 'path2')

    osutils.Touch(path1)
    osutils.SafeMakedirs(subdir)

    filelib.Copy(path1, path2)
    self.assertExists(path2)

  def testCopyIntoNewDir(self):
    """Copy a file into a dir that does not yet exist."""
    path1 = os.path.join(self.tempdir, 'path1')
    subdir = os.path.join(self.tempdir, 'subdir')
    path2 = os.path.join(subdir, 'path2')

    osutils.Touch(path1)

    filelib.Copy(path1, path2)
    self.assertExists(path2)

  def testCopyRelative(self):
    """Copy a file using relative destination."""
    path1 = os.path.join(self.tempdir, 'path1')
    path2 = os.path.join(self.tempdir, 'path2')
    relative_path = os.path.basename(path2)

    os.chdir(self.tempdir)
    osutils.Touch(path1)

    filelib.Copy(path1, relative_path)
    self.assertExists(path2)

  def testCopyFileSegment(self):
    """Test copying on a simple file segment."""
    a = os.path.join(self.tempdir, 'a.txt')
    osutils.WriteFile(a, '789')
    b = os.path.join(self.tempdir, 'b.txt')
    osutils.WriteFile(b, '0123')
    filelib.CopyFileSegment(a, 'r', 2, b, 'r+', in_seek=1)
    self.assertEqual(osutils.ReadFile(b), '8923')
