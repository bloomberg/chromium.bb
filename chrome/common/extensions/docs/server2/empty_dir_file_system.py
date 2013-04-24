# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from file_system import FileNotFoundError, FileSystem, StatInfo
from future import Future

class EmptyDirFileSystem(FileSystem):
  '''A FileSystem with empty directories. Useful to inject places to disable
  features such as samples.
  '''
  def Read(self, paths, binary=False):
    result = {}
    for path in paths:
      if not path.endswith('/'):
        raise FileNotFoundError('EmptyDirFileSystem cannot read %s' % path)
      result[path] = []
    return Future(value=result)

  def Stat(self, path):
    if not path.endswith('/'):
      raise FileNotFoundError('EmptyDirFileSystem cannot stat %s' % path)
    return StatInfo(0, child_versions=[])
