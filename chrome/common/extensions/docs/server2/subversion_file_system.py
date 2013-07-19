# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath
import xml.dom.minidom as xml
from xml.parsers.expat import ExpatError

from appengine_url_fetcher import AppEngineUrlFetcher
from docs_server_utils import StringIdentity
from file_system import FileSystem, FileNotFoundError, StatInfo, ToUnicode
from future import Future
import svn_constants
import url_constants

class _AsyncFetchFuture(object):
  def __init__(self, paths, fetcher, binary, args=None):
    def apply_args(path):
      return path if args is None else '%s?%s' % (path, args)
    # A list of tuples of the form (path, Future).
    self._fetches = [(path, fetcher.FetchAsync(apply_args(path)))
                     for path in paths]
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
      try:
        result = future.Get()
      except Exception as e:
        raise FileNotFoundError(
            'Error when fetching %s for Get: %s' % (path, e))
      if result.status_code == 404:
        raise FileNotFoundError('Got 404 when fetching %s for Get' % path)
      elif path.endswith('/'):
        self._value[path] = self._ListDir(result.content)
      elif not self._binary:
        self._value[path] = ToUnicode(result.content)
      else:
        self._value[path] = result.content
    if self._error is not None:
      raise self._error
    return self._value

class SubversionFileSystem(FileSystem):
  '''Class to fetch resources from src.chromium.org.
  '''
  @staticmethod
  def Create(branch='trunk', revision=None):
    if branch == 'trunk':
      svn_path = 'trunk/src/%s' % svn_constants.EXTENSIONS_PATH
    else:
      svn_path = 'branches/%s/src/%s' % (branch, svn_constants.EXTENSIONS_PATH)
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

  def Read(self, paths, binary=False):
    args = None
    if self._revision is not None:
      # |fetcher| gets from svn.chromium.org which uses p= for version.
      args = 'p=%s' % self._revision
    return Future(delegate=_AsyncFetchFuture(paths,
                                             self._file_fetcher,
                                             binary,
                                             args=args))

  def _ParseHTML(self, html):
    '''Unfortunately, the viewvc page has a stray </div> tag, so this takes care
    of all mismatched tags.
    '''
    try:
      return xml.parseString(html)
    except ExpatError as e:
      return self._ParseHTML('\n'.join(
          line for (i, line) in enumerate(html.split('\n'))
          if e.lineno != i + 1))

  def _CreateStatInfo(self, html):
    def inner_text(node):
      '''Like node.innerText in JS DOM, but strips surrounding whitespace.
      '''
      text = []
      if node.nodeValue:
        text.append(node.nodeValue)
      if hasattr(node, 'childNodes'):
        for child_node in node.childNodes:
          text.append(inner_text(child_node))
      return ''.join(text).strip()

    dom = self._ParseHTML(html)

    # Try all of the tables until we find the one that contains the data.
    for table in dom.getElementsByTagName('table'):
      # Within the table there is a list of files. However, there may be some
      # things beforehand; a header, "parent directory" list, etc. We will deal
      # with that below by being generous and just ignoring such rows.
      rows = table.getElementsByTagName('tr')
      child_versions = {}

      for row in rows:
        # Within each row there are probably 5 cells; name, version, age,
        # author, and last log entry. Maybe the columns will change; we're at
        # the mercy viewvc, but this constant can be easily updated.
        elements = row.getElementsByTagName('td')
        if len(elements) != 5:
          continue
        name_element, version_element, _, __, ___ = elements

        name = inner_text(name_element)  # note: will end in / for directories
        try:
          version = int(inner_text(version_element))
        except ValueError:
          continue
        child_versions[name] = version

      if not child_versions:
        continue

      # Parent version is max version of all children, since it's SVN.
      parent_version = max(child_versions.values())

      # All versions in StatInfo need to be strings.
      return StatInfo(str(parent_version),
                      dict((path, str(version))
                           for path, version in child_versions.iteritems()))

    # Bleh, but, this data is so unreliable. There are actually some empty file
    # listings caused by git/svn/something not cleaning up empty dirs.
    return StatInfo('0', {})

  def Stat(self, path):
    directory, filename = posixpath.split(path)
    directory += '/'
    if self._revision is not None:
      # |stat_fetch| uses viewvc which uses pathrev= for version.
      directory += '?pathrev=%s' % self._revision
    result = self._stat_fetcher.Fetch(directory)
    if result.status_code == 404:
      raise FileNotFoundError(
          'Got 404 when fetching %s from %s for Stat' % (path, directory))
    stat_info = self._CreateStatInfo(result.content)
    if path.endswith('/'):
      return stat_info
    if filename not in stat_info.child_versions:
      raise FileNotFoundError('%s was not in child versions' % filename)
    return StatInfo(stat_info.child_versions[filename])

  def GetIdentity(self):
    # NOTE: no revision here, consider it just an implementation detail of the
    # file version that is handled by Stat.
    return '@'.join((self.__class__.__name__, StringIdentity(self._svn_path)))
