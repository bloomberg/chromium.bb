# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from third_party.json_schema_compiler.memoize import memoize


class HostFileSystemIterator(object):
  '''Provides methods for iterating through host file systems, in both
  ascending (oldest to newest version) and descending order.
  '''

  def __init__(self, file_system_creator, host_file_system, branch_utility):
    self._file_system_creator = file_system_creator
    self._host_file_system = host_file_system
    self._branch_utility = branch_utility

  @memoize
  def _GetFileSystem(self, branch):
    '''To avoid overwriting the persistent data store entry for the 'trunk'
    host file system, hold on to a reference of this file system and return it
    instead of creating a file system for 'trunk'.
      Also of note: File systems are going to be iterated over multiple times
    at each call of ForEach, but the data isn't going to change between calls.
    Use |branch| to memoize the created file systems.
    '''
    if branch == 'trunk':
      # Don't create a new file system for trunk, since there is a bug with the
      # current architecture and design of HostFileSystemCreator, where
      # creating 'trunk' ignores the pinned revision (in fact, it bypasses
      # every difference including whether the file system is patched).
      # TODO(kalman): Fix HostFileSystemCreator and update this comment.
      return self._host_file_system
    return self._file_system_creator.Create(branch)

  def _ForEach(self, channel_info, callback, get_next):
    '''Iterates through a sequence of file systems defined by |get_next| until
    |callback| returns False, or until the end of the sequence of file systems
    is reached. Returns the BranchUtility.ChannelInfo of the last file system
    for which |callback| returned True.
    '''
    last_true = None
    while channel_info is not None:
      file_system = self._GetFileSystem(channel_info.branch)
      if not callback(file_system, channel_info):
        return last_true
      last_true = channel_info
      channel_info = get_next(channel_info)
    return last_true

  def Ascending(self, channel_info, callback):
    return self._ForEach(channel_info, callback, self._branch_utility.Newer)

  def Descending(self, channel_info, callback):
    return self._ForEach(channel_info, callback, self._branch_utility.Older)
