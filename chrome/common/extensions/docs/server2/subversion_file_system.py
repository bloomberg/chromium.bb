# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import xml.dom.minidom as xml

from file_system import FileSystem
from future import Future

class SubversionFileSystem(FileSystem):
  """Class to fetch resources from src.chromium.org.
  """
  def __init__(self, fetcher):
    self._fetcher = fetcher

  def Read(self, paths):
    return Future(delegate=_AsyncFetchFuture(paths, self._fetcher))

  def Stat(self, path):
    directory = path.rsplit('/', 1)[0]
    dir_html = self._fetcher.Fetch(directory + '/').content
    return self.StatInfo(int(re.search('([0-9]+)', dir_html).group(0)))

class _AsyncFetchFuture(object):
  def __init__(self, paths, fetcher):
    # A list of tuples of the form (path, Future).
    self._fetches = []
    self._value = {}
    self._error = None
    self._fetches = [(path, fetcher.FetchAsync(path)) for path in paths]

  def _ListDir(self, directory):
    dom = xml.parseString(directory)
    files = [elem.childNodes[0].data for elem in dom.getElementsByTagName('a')]
    if '..' in files:
      files.remove('..')
    return files

  def Get(self):
    for path, future in self._fetches:
      result = future.Get()
      if result.status_code == 404:
        self._value[path] = None
      elif path.endswith('/'):
        self._value[path] = self._ListDir(result.content)
      else:
        self._value[path] = result.content
    if self._error is not None:
      raise self._error
    return self._value

