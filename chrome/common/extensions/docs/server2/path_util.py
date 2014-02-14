# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath


# TODO(kalman): Write a Path class and use that everywhere rather than a
# utility class.


def IsDirectory(path):
  '''Returns whether |path| should be considered a directory.
  '''
  # This assertion is sprinkled throughout the code base.
  AssertIsValid(path)
  return path in ('', '.', '..') or path.endswith('/') or path.endswith('/..')


def IsValid(path):
  '''Returns whether |path| is a valid path for the purposes of the docserver.
  Paths may not start with /, must be posix paths, and for sanity shouldn't
  repeat the path separator //.
  '''
  return not path.startswith('/') and not '\\' in path and not '//' in path


def AssertIsValid(path):
  assert IsValid(path), 'Path "%s" is invalid' % path


def Join(*paths):
  assert all(IsValid(path) for path in paths), paths
  return posixpath.join(*paths)


def SplitParent(path):
  '''Returns the parent directory and base name of |path| in a tuple.
  Any trailing slash of |path| is preserved, such that the parent of
  '/hello/world/' is '/hello' and the base is 'world/'.
  '''
  parent, base = posixpath.split(path.rstrip('/'))
  if path.endswith('/'):
    base += '/'
  return parent, base


def ToDirectory(path):
  '''Returns a string representing |path| as a directory, that is,
  IsDirectory(result) is True (and does not fail assertions). If |path| is
  already a directory then this is a no-op.
  '''
  return path if IsDirectory(path) else (path + '/')


def AssertIsDirectory(path):
  assert IsDirectory(path), '"%s" is not a directory' % path


def AssertIsFile(path):
  assert not IsDirectory(path), '"%s" is not a file' % path
