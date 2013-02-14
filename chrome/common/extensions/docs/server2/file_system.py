# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

class FileNotFoundError(Exception):
  def __init__(self, filename):
    Exception.__init__(self, filename)

class StatInfo(object):
  """The result of calling Stat on a FileSystem.
  """
  def __init__(self, version, child_versions=None):
    self.version = version
    self.child_versions = child_versions

def _ToUnicode(data):
  try:
    return unicode(data, 'utf-8')
  except:
    return unicode(data, 'latin-1')

class FileSystem(object):
  """A FileSystem interface that can read files and directories.
  """

  def Read(self, paths, binary=False):
    """Reads each file in paths and returns a dictionary mapping the path to the
    contents. If a path in paths ends with a '/', it is assumed to be a
    directory, and a list of files in the directory is mapped to the path.

    If binary=False, the contents of each file will be unicode parsed as utf-8,
    and failing that as latin-1 (some extension docs use latin-1). If
    binary=True then the contents will be a str.
    """
    raise NotImplementedError()

  def ReadSingle(self, path, binary=False):
    """Reads a single file from the FileSystem.
    """
    return self.Read([path], binary=binary).Get()[path]

  # TODO(cduvall): Allow Stat to take a list of paths like Read.
  def Stat(self, path):
    """Returns a |StatInfo| object containing the version of |path|. If |path|
    is a directory, |StatInfo| will have the versions of all the children of
    the directory in |StatInfo.child_versions|.
    """
    raise NotImplementedError()
