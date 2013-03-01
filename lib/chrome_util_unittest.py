#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                '..', '..'))
from chromite.lib import cros_test_lib
from chromite.lib import chrome_util

# pylint: disable=W0212,W0233

# Convenience alias
Dir = cros_test_lib.Directory

class CopyTest(cros_test_lib.TempDirTestCase):
  def setUp(self):
    self.src_base = os.path.join(self.tempdir, 'src_base')
    self.dest_base = os.path.join(self.tempdir, 'dest_base')
    os.mkdir(self.src_base)
    os.mkdir(self.dest_base)
    self.copier = chrome_util.Copier()

  def _CopyAndVerify(self, path, src_struct, dest_struct, error=None,
                     strict=False, sloppy=False):
    cros_test_lib.CreateOnDiskHierarchy(self.src_base, src_struct)
    if error:
      self.assertRaises(error, path.Copy, self.src_base, self.dest_base, None,
                        strict, sloppy)
      return

    path.Copy(self.src_base, self.dest_base, self.copier, strict, sloppy)
    cros_test_lib.VerifyOnDiskHierarchy(self.dest_base, dest_struct)


class FileCopyTest(CopyTest):
  """Testing the file copying/globbing/renaming functionality of Path class."""

  ELEMENT_SRC_NAME = 'file1'
  ELEMENT_SRC = ELEMENT_SRC_NAME
  ELEMENTS_SRC = ['file1', 'file2', 'file3', 'monkey1', 'monkey2', 'monkey3']
  ELEMENTS_GLOB = 'file*'
  DIR_SRC_NAME = 'dir_src'

  ELEMENT_DEST_NAME = 'file_dest'
  ELEMENT_DEST = ELEMENT_DEST_NAME
  ELEMENTS_DEST = ['file1', 'file2', 'file3']
  DIR_DEST_NAME = 'dir_dest'

  MATCH_NOTHING_GLOB = 'match_nothing'
  BAD_ELEMENTS = ['wont match1', 'wont match2']

  def testSurfaceCopy(self):
    """Copying an element from the root."""
    src_struct = self.ELEMENTS_SRC
    dest_struct = [self.ELEMENT_SRC]
    path = chrome_util.Path(self.ELEMENT_SRC_NAME)
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testStrictAndSloppy(self):
    """Test that strict and sloppy copies fail with an AssertionError."""
    src_struct = self.ELEMENTS_SRC
    dest_struct = [self.ELEMENT_SRC]
    path = chrome_util.Path(self.ELEMENT_SRC_NAME)
    self._CopyAndVerify(path, src_struct, dest_struct, error=AssertionError,
                        sloppy=True, strict=True)

  def testSurfaceRename(self):
    """"Renaming of an element from the root."""
    src_struct = self.ELEMENTS_SRC
    dest_struct = [self.ELEMENT_DEST]
    path = chrome_util.Path(self.ELEMENT_SRC_NAME, dest=self.ELEMENT_DEST_NAME)
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testOneLevelDeepCopy(self):
    """Copying an element inside a directory."""
    src_struct = [Dir(self.DIR_SRC_NAME, self.ELEMENTS_SRC)]
    dest_struct = [Dir(self.DIR_SRC_NAME, [self.ELEMENT_SRC])]
    path = chrome_util.Path(
        os.path.join(self.DIR_SRC_NAME, self.ELEMENT_SRC_NAME))
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testOneLevelDeepRename(self):
    """Renaming of an element inside a directory."""
    src_struct = [Dir(self.DIR_SRC_NAME, self.ELEMENTS_SRC)]
    dest_struct = [Dir(self.DIR_SRC_NAME, [self.ELEMENT_DEST])]

    path = chrome_util.Path(
        os.path.join(self.DIR_SRC_NAME, self.ELEMENT_SRC_NAME),
        dest=os.path.join(self.DIR_SRC_NAME, self.ELEMENT_DEST_NAME))
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testOneLevelDeepDirRename(self):
    """Renaming of an element and its containing directory."""
    src_struct = [Dir(self.DIR_SRC_NAME, self.ELEMENTS_SRC)]
    dest_struct = [Dir(self.DIR_DEST_NAME, [self.ELEMENT_DEST])]

    path = chrome_util.Path(
        os.path.join(self.DIR_SRC_NAME, self.ELEMENT_SRC_NAME),
        dest=os.path.join(self.DIR_DEST_NAME, self.ELEMENT_DEST_NAME))
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testSingleGlob(self):
    """Glob matching one element."""
    src_struct = dest_struct = [Dir(self.DIR_SRC_NAME, [self.ELEMENT_SRC])]
    path = chrome_util.Path(os.path.join(self.DIR_SRC_NAME, self.ELEMENTS_GLOB))
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testSingleGlobToDirectory(self):
    """Glob matching one element and dest directory provided."""
    src_struct = [Dir(self.DIR_SRC_NAME, [self.ELEMENT_SRC])]
    dest_struct = [Dir(self.DIR_DEST_NAME, [self.ELEMENT_SRC])]
    path = chrome_util.Path(os.path.join(self.DIR_SRC_NAME, self.ELEMENTS_GLOB),
                            dest=(self.DIR_DEST_NAME + os.sep))
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testMultiGlob(self):
    """Glob matching one file and dest directory provided."""
    src_struct = [Dir(self.DIR_SRC_NAME, self.ELEMENTS_SRC)]
    dest_struct = [Dir(self.DIR_SRC_NAME, self.ELEMENTS_DEST)]

    path = chrome_util.Path(os.path.join(self.DIR_SRC_NAME, self.ELEMENTS_GLOB))
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testMultiGlobToDirectory(self):
    """Glob matching multiple elements and dest directory provided."""
    src_struct = [Dir(self.DIR_SRC_NAME, self.ELEMENTS_SRC)]
    dest_struct = [Dir(self.DIR_DEST_NAME, self.ELEMENTS_DEST)]
    path = chrome_util.Path(os.path.join(self.DIR_SRC_NAME, self.ELEMENTS_GLOB),
                            dest=(self.DIR_DEST_NAME + os.sep))
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testGlobReturnsMultipleError(self):
    """Glob returns multiple results but dest does not end with '/'."""
    src_struct = self.ELEMENTS_SRC
    path = chrome_util.Path(self.ELEMENTS_GLOB, dest=self.DIR_DEST_NAME)
    self._CopyAndVerify(
        path, src_struct, None, error=chrome_util.MultipleMatchError)

  def testNoElementError(self):
    """A path that is not optional cannot be found."""
    src_struct = self.BAD_ELEMENTS
    path = chrome_util.Path(self.ELEMENT_SRC_NAME)
    self._CopyAndVerify(
        path, src_struct, [], error=chrome_util.MissingPathError)

  def testNoGlobError(self):
    """A glob that is not optional matches nothing."""
    src_struct = self.ELEMENTS_SRC
    path = chrome_util.Path(self.MATCH_NOTHING_GLOB)
    self._CopyAndVerify(
        path, src_struct, [], error=chrome_util.MissingPathError)

  def testNonDirError(self):
    """Test case where a file pattern matches a directory."""
    src_struct = ['file1/']
    dest_struct = []
    path = chrome_util.Path('file1')
    self._CopyAndVerify(path, src_struct, dest_struct,
                        error=chrome_util.MustNotBeDirError)

  def testElementOptional(self):
    """A path cannot be found but is optional."""
    src_struct = self.BAD_ELEMENTS
    dest_struct = []
    path = chrome_util.Path(self.ELEMENT_SRC_NAME, optional=True)
    self._CopyAndVerify(path, src_struct, dest_struct)

  def testOptionalGlob(self):
    """A glob matches nothing but is optional."""
    src_struct = self.ELEMENTS_SRC
    dest_struct = []
    path = chrome_util.Path(self.MATCH_NOTHING_GLOB, optional=True)
    self._CopyAndVerify(path, src_struct, dest_struct)


class SloppyFileCopyTest(FileCopyTest):
  """Test file copies with sloppy=True"""

  def _CopyAndVerify(self, path, src_struct, dest_struct, error=None,
                     strict=False, sloppy=True):
    if error is chrome_util.MissingPathError:
      error = None
    CopyTest._CopyAndVerify(self, path, src_struct, dest_struct, error=error,
                            strict=strict, sloppy=sloppy)


class DirCopyTest(FileCopyTest):
  """Testing directory copying/globbing/renaming functionality of Path class."""

  FILES = ['file1', 'file2', 'file3']
  ELEMENT_SRC_NAME = 'monkey1/'
  ELEMENT_SRC = Dir(ELEMENT_SRC_NAME, FILES)
  ELEMENTS_SRC = [
      Dir('monkey1', FILES) , Dir('monkey2', FILES), Dir('monkey3', FILES),
      Dir('foon1', []), Dir('foon2', []), Dir('foon3', [])]
  ELEMENTS_GLOB = 'monkey*'
  DIR_SRC_NAME = 'dir_src'

  ELEMENT_DEST_NAME = 'monkey_dest'
  ELEMENT_DEST = Dir(ELEMENT_DEST_NAME, FILES)
  ELEMENTS_DEST = [
      Dir('monkey1', FILES) , Dir('monkey2', FILES), Dir('monkey3', FILES)]
  DIR_DEST_NAME = 'dir_dest'


class SloppyDirCopyTest(SloppyFileCopyTest, DirCopyTest):
  """Test directory copies with sloppy=True"""


if __name__ == '__main__':
  cros_test_lib.main()
