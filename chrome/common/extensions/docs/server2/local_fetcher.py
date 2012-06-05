# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

class LocalFetcher(object):
  """Class to fetch resources from local filesystem.
  """
  def __init__(self, base_path):
    self.base_path = base_path

  class _Resource(object):
    def __init__(self):
      self.content = ''
      self.headers = {}

  def _ReadFile(self, filename):
    with open(filename, 'r') as f:
      return f.read()

  def FetchResource(self, branch, path):
    real_path = os.path.join(*path.split('/'))
    result = self._Resource()
    logging.info('Reading: ' + os.path.join(self.base_path, real_path))
    result.content = self._ReadFile(os.path.join(self.base_path, real_path))
    return result
