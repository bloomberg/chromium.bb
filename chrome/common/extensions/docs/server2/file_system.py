# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath
import traceback

from future import Future
from path_util import (
    AssertIsDirectory, AssertIsValid, IsDirectory, IsValid, SplitParent,
    ToDirectory)


class _BaseFileSystemException(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @classmethod
  def RaiseInFuture(cls, message):
    stack = traceback.format_stack()
    def boom(): raise cls('%s. Creation stack:\n%s' % (message, ''.join(stack)))
    return Future(callback=boom)


class FileNotFoundError(_BaseFileSystemException):
  '''Raised when a file isn't found for read or stat.
  '''
  def __init__(self, filename):
    _BaseFileSystemException.__init__(self, filename)


class FileSystemError(_BaseFileSystemException):
  '''Raised on when there are errors reading or statting files, such as a
  network timeout.
  '''
  def __init__(self, filename):
    _BaseFileSystemException.__init__(self, filename)


class StatInfo(object):
  '''The result of calling Stat on a FileSystem.
  '''
  def __init__(self, version, child_versions=None):
    if child_versions:
      assert all(IsValid(path) for path in child_versions.iterkeys()), \
             child_versions
    self.version = version
    self.child_versions = child_versions

  def __eq__(self, other):
    return (isinstance(other, StatInfo) and
            self.version == other.version and
            self.child_versions == other.child_versions)

  def __ne__(self, other):
    return not (self == other)

  def __str__(self):
    return '{version: %s, child_versions: %s}' % (self.version,
                                                  self.child_versions)

  def __repr__(self):
    return str(self)


class FileSystem(object):
  '''A FileSystem interface that can read files and directories.
  '''
  def Read(self, paths, skip_not_found=False):
    '''Reads each file in paths and returns a dictionary mapping the path to the
    contents. If a path in paths ends with a '/', it is assumed to be a
    directory, and a list of files in the directory is mapped to the path.

    The contents will be a str.

    If any path cannot be found:
      - If |skip_not_found| is True, the resulting object will not contain any
        mapping for that path.
      - Otherwise, and by default, a FileNotFoundError is raised. This is
        guaranteed to only happen once the Future has been resolved (Get()
        called).

    For any other failure, raises a FileSystemError.
    '''
    raise NotImplementedError(self.__class__)

  def ReadSingle(self, path):
    '''Reads a single file from the FileSystem. Returns a Future with the same
    rules as Read(). If |path| is not found raise a FileNotFoundError on Get().
    '''
    AssertIsValid(path)
    read_single = self.Read([path])
    return Future(callback=lambda: read_single.Get()[path])

  def Exists(self, path):
    '''Returns a Future to the existence of |path|; True if |path| exists,
    False if not. This method will not throw a FileNotFoundError unlike
    the Read* methods, however it may still throw a FileSystemError.

    There are several ways to implement this method via the interface but this
    method exists to do so in a canonical and most efficient way for caching.
    '''
    AssertIsValid(path)
    if path == '':
      # There is always a root directory.
      return Future(value=True)

    parent, base = SplitParent(path)
    list_future = self.ReadSingle(ToDirectory(parent))
    def resolve():
      try:
        return base in list_future.Get()
      except FileNotFoundError:
        return False
    return Future(callback=resolve)

  def Refresh(self):
    '''Asynchronously refreshes the content of the FileSystem, returning a
    future to its completion.
    '''
    raise NotImplementedError(self.__class__)

  # TODO(cduvall): Allow Stat to take a list of paths like Read.
  def Stat(self, path):
    '''Returns a |StatInfo| object containing the version of |path|. If |path|
    is a directory, |StatInfo| will have the versions of all the children of
    the directory in |StatInfo.child_versions|.

    If the path cannot be found, raises a FileNotFoundError.
    For any other failure, raises a FileSystemError.
    '''
    raise NotImplementedError(self.__class__)

  def StatAsync(self, path):
    '''Bandaid for a lack of an async Stat function. Stat() should be async
    by default but for now just let implementations override this if they like.
    '''
    return Future(callback=lambda: self.Stat(path))

  def GetIdentity(self):
    '''The identity of the file system, exposed for caching classes to
    namespace their caches. this will usually depend on the configuration of
    that file system - e.g. a LocalFileSystem with a base path of /var is
    different to that of a SubversionFileSystem with a base path of /bar, is
    different to a LocalFileSystem with a base path of /usr.
    '''
    raise NotImplementedError(self.__class__)

  def Walk(self, root):
    '''Recursively walk the directories in a file system, starting with root.

    Behaviour is very similar to os.walk from the standard os module, yielding
    (base, dirs, files) recursively, where |base| is the base path of |files|,
    |dirs| relative to |root|, and |files| and |dirs| the list of files/dirs in
    |base| respectively.

    Note that directories will always end with a '/', files never will.

    If |root| cannot be found, raises a FileNotFoundError.
    For any other failure, raises a FileSystemError.
    '''
    AssertIsDirectory(root)
    basepath = root

    def walk(root):
      AssertIsDirectory(root)
      dirs, files = [], []

      for f in self.ReadSingle(root).Get():
        if IsDirectory(f):
          dirs.append(f)
        else:
          files.append(f)

      yield root[len(basepath):].rstrip('/'), dirs, files

      for d in dirs:
        for walkinfo in walk(root + d):
          yield walkinfo

    for walkinfo in walk(root):
      yield walkinfo

  def __eq__(self, other):
    return (isinstance(other, FileSystem) and
            self.GetIdentity() == other.GetIdentity())

  def __ne__(self, other):
    return not (self == other)

  def __repr__(self):
    return '<%s>' % type(self).__name__

  def __str__(self):
    return repr(self)
