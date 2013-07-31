# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class FileNotFoundError(Exception):
  def __init__(self, filename):
    Exception.__init__(self, filename)

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

def ToUnicode(data):
  '''Returns the str |data| as a unicode object. It's expected to be utf8, but
  there are also latin-1 encodings in there for some reason. Fall back to that.
  '''
  try:
    return unicode(data, 'utf-8')
  except:
    return unicode(data, 'latin-1')

class FileSystem(object):
  '''A FileSystem interface that can read files and directories.
  '''
  def Read(self, paths, binary=False):
    '''Reads each file in paths and returns a dictionary mapping the path to the
    contents. If a path in paths ends with a '/', it is assumed to be a
    directory, and a list of files in the directory is mapped to the path.

    If binary=False, the contents of each file will be unicode parsed as utf-8,
    and failing that as latin-1 (some extension docs use latin-1). If
    binary=True then the contents will be a str.
    '''
    raise NotImplementedError(self.__class__)

  def ReadSingle(self, path, binary=False):
    '''Reads a single file from the FileSystem.
    '''
    return self.Read([path], binary=binary).Get()[path]

  # TODO(cduvall): Allow Stat to take a list of paths like Read.
  def Stat(self, path):
    '''Returns a |StatInfo| object containing the version of |path|. If |path|
    is a directory, |StatInfo| will have the versions of all the children of
    the directory in |StatInfo.child_versions|.
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
    '''
    basepath = root.rstrip('/') + '/'

    def walk(root):
      if not root.endswith('/'):
        root += '/'

      dirs, files = [], []

      for f in self.ReadSingle(root):
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
