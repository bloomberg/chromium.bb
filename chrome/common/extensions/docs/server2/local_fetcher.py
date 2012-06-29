# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

class LocalFetcher(object):
  """Class to fetch resources from local filesystem.
  """
  def __init__(self, base_path):
    self._base_path = self._ConvertToFilepath(base_path)

  def _ConvertToFilepath(self, path):
    return path.replace('/', os.sep)

  class _Response(object):
    """Response object matching what is returned from urlfetch.
    """
    def __init__(self, content):
      self.content = content
      self.headers = {}

  def _ReadFile(self, filename):
    path = os.path.join(self._base_path, filename)
    with open(path, 'r') as f:
      return f.read()

  def FetchResource(self, path):
    # A response object is returned to match the behavior of urlfetch.
    # See: developers.google.com/appengine/docs/python/urlfetch/responseobjects
    return self._Response(self._ReadFile(self._ConvertToFilepath(path)))
