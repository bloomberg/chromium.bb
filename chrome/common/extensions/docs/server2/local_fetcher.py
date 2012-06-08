# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

class LocalFetcher(object):
  """Class to fetch resources from local filesystem.
  """
  def __init__(self, base_path):
    self._base_path = self._ConvertToFilepath(base_path)

  def _ConvertToFilepath(self, path):
    return path.replace('/', os.sep)

  class _Resource(object):
    def __init__(self, content):
      self.content = content
      self.headers = {}

  def _ReadFile(self, filename):
    path = os.path.join(self._base_path, filename)
    logging.info('Reading: ' + path)
    with open(path, 'r') as f:
      return f.read()

  def FetchResource(self, path):
    return self._Resource(self._ReadFile(self._ConvertToFilepath(path)))
