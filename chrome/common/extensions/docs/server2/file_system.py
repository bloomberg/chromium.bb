# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class FileSystem(object):
  """A FileSystem interface that can read files and directories.
  """
  class StatInfo(object):
    """The result of calling Stat on a FileSystem.
    """
    def __init__(self, version):
      self.version = version

  def Read(self, paths):
    """Reads each file in paths and returns a dictionary mapping the path to the
    contents. If a path in paths ends with a '/', it is assumed to be a
    directory, and a list of files in the directory is mapped to the path.
    """
    raise NotImplementedError()

  def Stat(self, path):
    """Gets the version number of |path| if it is a directory, or the parent
    directory if it is a file.
    """
    raise NotImplementedError()
