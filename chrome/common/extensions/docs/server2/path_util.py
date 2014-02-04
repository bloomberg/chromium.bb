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
  assert not path.startswith('/'), path
  return path == '' or path.endswith('/')


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
