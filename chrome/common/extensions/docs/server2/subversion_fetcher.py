# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import xml.dom.minidom as xml

SUBVERSION_URL = 'http://src.chromium.org/chrome'
TRUNK_URL = SUBVERSION_URL + '/trunk'
BRANCH_URL = SUBVERSION_URL + '/branches'

class SubversionFetcher(object):
  """Class to fetch resources from src.chromium.org.
  """
  def __init__(self, branch, base_path, url_fetcher):
    self._base_path = self._GetURLFromBranch(branch) + '/' + base_path
    self._url_fetcher = url_fetcher

  def _GetURLFromBranch(self, branch):
    if branch == 'trunk':
      return TRUNK_URL + '/src'
    return BRANCH_URL + '/' + branch + '/src'

  def _ListDir(self, directory):
    dom = xml.parseString(directory)
    files = [elem.childNodes[0].data for elem in dom.getElementsByTagName('a')]
    if '..' in files:
      files.remove('..')
    return files

  def _RecursiveList(self, files, base):
    all_files = []
    for filename in files:
      if filename.endswith('/'):
        dir_name = base + '/' + filename.split('/')[-2]
        all_files.extend(self.ListDirectory(dir_name, True).content)
      else:
        all_files.append(base + '/' + filename)
    return all_files

  def ListDirectory(self, path, recursive=False):
    result = self._url_fetcher.fetch(self._base_path + '/' + path)
    result.content = self._ListDir(result.content)
    if recursive:
      result.content = self._RecursiveList(result.content, path)
    return result

  def FetchResource(self, path):
    return self._url_fetcher.fetch(self._base_path + '/' + path)

