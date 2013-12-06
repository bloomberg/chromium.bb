# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from future import Gettable, Future


class _BaseFileSystemException(Exception):
  def __init__(self, message):
    Exception.__init__(self, message)

  @classmethod
  def RaiseInFuture(cls, message):
    def boom(): raise cls(message)
    return Future(delegate=Gettable(boom))


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
  def Read(self, paths):
    '''Reads each file in paths and returns a dictionary mapping the path to the
    contents. If a path in paths ends with a '/', it is assumed to be a
    directory, and a list of files in the directory is mapped to the path.

    The contents will be a str.

    If any path cannot be found, raises a FileNotFoundError. This is guaranteed
    to only happen once the Future has been resolved (Get() called).

    For any other failure, raises a FileSystemError.
    '''
    raise NotImplementedError(self.__class__)

  def ReadSingle(self, path):
    '''Reads a single file from the FileSystem. Returns a Future with the same
    rules as Read().
    '''
    read_single = self.Read([path])
    return Future(delegate=Gettable(lambda: read_single.Get()[path]))

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
    Emulates os.walk from the standard os module.

    If the root cannot be found, raises a FileNotFoundError.
    For any other failure, raises a FileSystemError.
    '''
    basepath = root.rstrip('/') + '/'

    def walk(root):
      if not root.endswith('/'):
        root += '/'

      dirs, files = [], []

      for f in self.ReadSingle(root).Get():
        if f.endswith('/'):
          dirs.append(f)
        else:
          files.append(f)

      yield root[len(basepath):].rstrip('/'), dirs, files

      for d in dirs:
        for walkinfo in walk(root + d):
          yield walkinfo

    for walkinfo in walk(root):
      yield walkinfo

  def __repr__(self):
    return '<%s>' % type(self).__name__

  def __str__(self):
    return repr(self)
