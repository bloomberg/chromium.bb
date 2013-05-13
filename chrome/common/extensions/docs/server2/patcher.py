# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class Patcher(object):
  def GetPatchedFiles(self, version=None):
    ''' Returns patched files as(added_files, deleted_files, modified_files)
    from the patchset specified by |version|.
    '''
    raise NotImplementedError()

  def GetVersion(self):
    ''' Returns patch version. Returns None when nothing is patched by the
    patcher.
    '''
    raise NotImplementedError()

  def Apply(self, paths, file_system, binary, version=None):
    ''' Apply the patch to added/modified files. Returns Future with patched
    data. Throws FileNotFoundError if |paths| contains deleted files.
    '''
    raise NotImplementedError()
