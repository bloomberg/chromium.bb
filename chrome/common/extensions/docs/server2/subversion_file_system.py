# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath
import traceback
import xml.dom.minidom as xml
from xml.parsers.expat import ExpatError

from appengine_url_fetcher import AppEngineUrlFetcher
from appengine_wrappers import IsDownloadError
from docs_server_utils import StringIdentity
from file_system import (
    FileNotFoundError, FileSystem, FileSystemError, StatInfo)
from future import Future
import url_constants


def _ParseHTML(html):
  '''Unfortunately, the viewvc page has a stray </div> tag, so this takes care
  of all mismatched tags.
  '''
  try:
    return xml.parseString(html)
  except ExpatError as e:
    return _ParseHTML('\n'.join(
        line for (i, line) in enumerate(html.split('\n'))
        if e.lineno != i + 1))

def _InnerText(node):
  '''Like node.innerText in JS DOM, but strips surrounding whitespace.
  '''
  text = []
  if node.nodeValue:
    text.append(node.nodeValue)
  if hasattr(node, 'childNodes'):
    for child_node in node.childNodes:
      text.append(_InnerText(child_node))
  return ''.join(text).strip()

def _CreateStatInfo(html):
  parent_version = None
  child_versions = {}

  # Try all of the tables until we find the ones that contain the data (the
  # directory and file versions are in different tables).
  for table in _ParseHTML(html).getElementsByTagName('table'):
    # Within the table there is a list of files. However, there may be some
    # things beforehand; a header, "parent directory" list, etc. We will deal
    # with that below by being generous and just ignoring such rows.
    rows = table.getElementsByTagName('tr')

    for row in rows:
      cells = row.getElementsByTagName('td')

      # The version of the directory will eventually appear in the soup of
      # table rows, like this:
      #
      # <tr>
      #   <td>Directory revision:</td>
      #   <td><a href=... title="Revision 214692">214692</a> (of...)</td>
      # </tr>
      #
      # So look out for that.
      if len(cells) == 2 and _InnerText(cells[0]) == 'Directory revision:':
        links = cells[1].getElementsByTagName('a')
        if len(links) != 2:
          raise FileSystemError('ViewVC assumption invalid: directory ' +
                                'revision content did not have 2 <a> ' +
                                ' elements, instead %s' % _InnerText(cells[1]))
        this_parent_version = _InnerText(links[0])
        int(this_parent_version)  # sanity check
        if parent_version is not None:
          raise FileSystemError('There was already a parent version %s, and ' +
                                ' we just found a second at %s' %
                                (parent_version, this_parent_version))
        parent_version = this_parent_version

      # The version of each file is a list of rows with 5 cells: name, version,
      # age, author, and last log entry. Maybe the columns will change; we're
      # at the mercy viewvc, but this constant can be easily updated.
      if len(cells) != 5:
        continue
      name_element, version_element, _, __, ___ = cells

      name = _InnerText(name_element)  # note: will end in / for directories
      try:
        version = int(_InnerText(version_element))
      except StandardError:
        continue
      child_versions[name] = str(version)

    if parent_version and child_versions:
      break

  return StatInfo(parent_version, child_versions)


class SubversionFileSystem(FileSystem):
  '''Class to fetch resources from src.chromium.org.
  '''
  @staticmethod
  def Create(branch='trunk', revision=None):
    if branch == 'trunk':
      svn_path = 'trunk/src'
    else:
      svn_path = 'branches/%s/src' % branch
    return SubversionFileSystem(
        AppEngineUrlFetcher('%s/%s' % (url_constants.SVN_URL, svn_path)),
        AppEngineUrlFetcher('%s/%s' % (url_constants.VIEWVC_URL, svn_path)),
        svn_path,
        revision=revision)

  def __init__(self, file_fetcher, stat_fetcher, svn_path, revision=None):
    self._file_fetcher = file_fetcher
    self._stat_fetcher = stat_fetcher
    self._svn_path = svn_path
    self._revision = revision

  def Read(self, paths, skip_not_found=False):
    args = None
    if self._revision is not None:
      # |fetcher| gets from svn.chromium.org which uses p= for version.
      args = 'p=%s' % self._revision

    def apply_args(path):
      return path if args is None else '%s?%s' % (path, args)

    def list_dir(directory):
      dom = xml.parseString(directory)
      files = [elem.childNodes[0].data
               for elem in dom.getElementsByTagName('a')]
      if '..' in files:
        files.remove('..')
      return files

    # A list of tuples of the form (path, Future).
    fetches = [(path, self._file_fetcher.FetchAsync(apply_args(path)))
               for path in paths]

    def resolve():
      value = {}
      for path, future in fetches:
        try:
          result = future.Get()
        except Exception as e:
          if skip_not_found and IsDownloadError(e): continue
          exc_type = (FileNotFoundError if IsDownloadError(e)
                                       else FileSystemError)
          raise exc_type('%s fetching %s for Get: %s' %
                         (type(e).__name__, path, traceback.format_exc()))
        if result.status_code == 404:
          if skip_not_found: continue
          raise FileNotFoundError(
              'Got 404 when fetching %s for Get, content %s' %
              (path, result.content))
        if result.status_code != 200:
          raise FileSystemError('Got %s when fetching %s for Get, content %s' %
              (result.status_code, path, result.content))
        if path.endswith('/'):
          value[path] = list_dir(result.content)
        else:
          value[path] = result.content
      return value
    return Future(callback=resolve)

  def Refresh(self):
    return Future(value=())

  def Stat(self, path):
    return self.StatAsync(path).Get()

  def StatAsync(self, path):
    directory, filename = posixpath.split(path)
    if self._revision is not None:
      # |stat_fetch| uses viewvc which uses pathrev= for version.
      directory += '?pathrev=%s' % self._revision

    result_future = self._stat_fetcher.FetchAsync(directory)
    def resolve():
      try:
        result = result_future.Get()
      except Exception as e:
        exc_type = FileNotFoundError if IsDownloadError(e) else FileSystemError
        raise exc_type('%s fetching %s for Stat: %s' %
                       (type(e).__name__, path, traceback.format_exc()))

      if result.status_code == 404:
        raise FileNotFoundError('Got 404 when fetching %s for Stat, '
                                'content %s' % (path, result.content))
      if result.status_code != 200:
        raise FileNotFoundError('Got %s when fetching %s for Stat, content %s' %
                                (result.status_code, path, result.content))

      stat_info = _CreateStatInfo(result.content)
      if stat_info.version is None:
        raise FileSystemError('Failed to find version of dir %s' % directory)
      if path == '' or path.endswith('/'):
        return stat_info
      if filename not in stat_info.child_versions:
        raise FileNotFoundError(
            '%s from %s was not in child versions for Stat' % (filename, path))
      return StatInfo(stat_info.child_versions[filename])

    return Future(callback=resolve)

  def GetIdentity(self):
    # NOTE: no revision here, since it would mess up the caching of reads. It
    # probably doesn't matter since all the caching classes will use the result
    # of Stat to decide whether to re-read - and Stat has a ceiling of the
    # revision - so when the revision changes, so might Stat. That is enough.
    return '@'.join((self.__class__.__name__, StringIdentity(self._svn_path)))
