# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import xml.dom.minidom as xml
from xml.parsers.expat import ExpatError

import file_system
from future import Future

class _AsyncFetchFuture(object):
  def __init__(self, paths, fetcher, binary):
    # A list of tuples of the form (path, Future).
    self._fetches = [(path, fetcher.FetchAsync(path)) for path in paths]
    self._value = {}
    self._error = None
    self._binary = binary

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
        raise file_system.FileNotFoundError(path)
      elif path.endswith('/'):
        self._value[path] = self._ListDir(result.content)
      elif not self._binary:
        self._value[path] = file_system._ToUnicode(result.content)
      else:
        self._value[path] = result.content
    if self._error is not None:
      raise self._error
    return self._value

class SubversionFileSystem(file_system.FileSystem):
  """Class to fetch resources from src.chromium.org.
  """
  def __init__(self, fetcher, stat_fetcher):
    self._fetcher = fetcher
    self._stat_fetcher = stat_fetcher

  def Read(self, paths, binary=False):
    return Future(delegate=_AsyncFetchFuture(paths, self._fetcher, binary))

  def _ParseHTML(self, html):
    """Unfortunately, the viewvc page has a stray </div> tag, so this takes care
    of all mismatched tags.
    """
    try:
      return xml.parseString(html)
    except ExpatError as e:
      return self._ParseHTML('\n'.join(
          line for (i, line) in enumerate(html.split('\n'))
          if e.lineno != i + 1))

  def _CreateStatInfo(self, html):
    dom = self._ParseHTML(html)
    # Brace yourself, this is about to get ugly. The page returned from viewvc
    # was not the prettiest.
    tds = dom.getElementsByTagName('td')
    a_list = []
    found = False
    dir_revision = None
    for td in tds:
      if found:
        dir_revision = td.getElementsByTagName('a')[0].firstChild.nodeValue
        found = False
      a_list.extend(td.getElementsByTagName('a'))
      if (td.firstChild is not None and
          td.firstChild.nodeValue == 'Directory revision:'):
        found = True
    child_revisions = {}
    for i, a in enumerate(a_list):
      if i + 1 >= len(a_list):
        break
      next_a = a_list[i + 1]
      name = a.getAttribute('name')
      if name:
        rev = next_a.getElementsByTagName('strong')[0]
        if 'file' in next_a.getAttribute('title'):
          child_revisions[name] = rev.firstChild.nodeValue
        else:
          child_revisions[name + '/'] = rev.firstChild.nodeValue
    return file_system.StatInfo(dir_revision, child_revisions)

  def Stat(self, path):
    directory = path.rsplit('/', 1)[0]
    result = self._stat_fetcher.Fetch(directory + '/')
    if result.status_code == 404:
      raise file_system.FileNotFoundError(path)
    stat_info = self._CreateStatInfo(result.content)
    if not path.endswith('/'):
      filename = path.rsplit('/', 1)[-1]
      if filename not in stat_info.child_versions:
        raise file_system.FileNotFoundError(path)
      stat_info.version = stat_info.child_versions[filename]
    return stat_info
