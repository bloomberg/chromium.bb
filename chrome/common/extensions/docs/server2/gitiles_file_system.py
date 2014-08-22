# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from base64 import b64decode
from itertools import izip
import json
import posixpath
import traceback

from appengine_url_fetcher import AppEngineUrlFetcher
from appengine_wrappers import IsDownloadError
from docs_server_utils import StringIdentity
from file_system import (FileNotFoundError,
                         FileSystem,
                         FileSystemError,
                         StatInfo)
from future import All, Future
from path_util import AssertIsValid, IsDirectory, ToDirectory
from third_party.json_schema_compiler.memoize import memoize
from url_constants import GITILES_BASE, GITILES_BRANCH_BASE


_JSON_FORMAT = '?format=JSON'
_TEXT_FORMAT = '?format=TEXT'


def _ParseGitilesJson(json_data):
  '''json.loads with fix-up for non-executable JSON. Use this to parse any JSON
  data coming from Gitiles views.
  '''
  return json.loads(json_data[json_data.find('{'):])


def _CreateStatInfo(json_data):
  '''Returns a StatInfo object comprised of the tree ID for |json_data|,
  as well as the tree IDs for the entries in |json_data|.
  '''
  tree = _ParseGitilesJson(json_data)
  return StatInfo(tree['id'],
                  dict((e['name'], e['id']) for e in tree['entries']))


class GitilesFileSystem(FileSystem):
  '''Class to fetch filesystem data from the Chromium project's gitiles
  service.
  '''
  @staticmethod
  def Create(branch='master', commit=None):
    if commit:
      base_url = '%s/%s' % (GITILES_BASE, commit)
    elif branch is 'master':
      base_url = '%s/master' % GITILES_BASE
    else:
      base_url = '%s/%s' % (GITILES_BRANCH_BASE, branch)
    return GitilesFileSystem(AppEngineUrlFetcher(), base_url, branch, commit)

  def __init__(self, fetcher, base_url, branch, commit):
    self._fetcher = fetcher
    self._base_url = base_url
    self._branch = branch
    self._commit = commit

  def _FetchAsync(self, url):
    '''Convenience wrapper for fetcher.FetchAsync, so callers don't
    need to use posixpath.join.
    '''
    AssertIsValid(url)
    return self._fetcher.FetchAsync('%s/%s' % (self._base_url, url))

  def _ResolveFetchContent(self, path, fetch_future, skip_not_found=False):
    '''Returns a future to cleanly resolve |fetch_future|.
    '''
    def handle(e):
      if skip_not_found and IsDownloadError(e):
        return None
      exc_type = FileNotFoundError if IsDownloadError(e) else FileSystemError
      raise exc_type('%s fetching %s for Get from %s: %s' %
          (type(e).__name__, path, self._base_url, traceback.format_exc()))

    def get_content(result):
      if result.status_code == 404:
        if skip_not_found:
          return None
        raise FileNotFoundError('Got 404 when fetching %s for Get from %s' %
                                (path, self._base_url))
      if result.status_code != 200:
        raise FileSystemError(
            'Got %s when fetching %s for Get from %s, content %s' %
            (result.status_code, path, self._base_url, result.content))
      return result.content
    return fetch_future.Then(get_content, handle)

  def Read(self, paths, skip_not_found=False):
    # Directory content is formatted in JSON in Gitiles as follows:
    #
    #   {
    #     "id": "12a5464de48d2c46bc0b2dc78fafed75aab554fa", # The tree ID.
    #     "entries": [
    #       {
    #         "mode": 33188,
    #         "type": "blob",
    #           "id": "ab971ca447bc4bce415ed4498369e00164d91cb6", # File ID.
    #         "name": ".gitignore"
    #       },
    #       ...
    #     ]
    #   }
    def list_dir(json_data):
      entries = _ParseGitilesJson(json_data).get('entries', [])
      return [e['name'] + ('/' if e['type'] == 'tree' else '') for e in entries]

    def fixup_url_format(path):
      # By default, Gitiles URLs display resources in HTML. To get resources
      # suitable for our consumption, a '?format=' string must be appended to
      # the URL. The format may be one of 'JSON' or 'TEXT' for directory or
      # text resources, respectively.
      return path + (_JSON_FORMAT if IsDirectory(path) else _TEXT_FORMAT)

    # A list of tuples of the form (path, Future).
    fetches = ((path, self._FetchAsync(fixup_url_format(path)))
               for path in paths)

    def parse_contents(results):
      value = {}
      for path, content in izip(paths, results):
        if content is None:
          continue
        # Gitiles encodes text content in base64 (see
        # http://tools.ietf.org/html/rfc4648 for info about base64).
        value[path] = (list_dir if IsDirectory(path) else b64decode)(content)
      return value
    return All(self._ResolveFetchContent(path, future, skip_not_found)
               for path, future in fetches).Then(parse_contents)

  def Refresh(self):
    return Future(value=())

  @memoize
  def _GetCommitInfo(self, key):
    '''Gets the commit information specified by |key|.

    The JSON view for commit info looks like:
      {
        "commit": "8fd578e1a7b142cd10a4387861f05fb9459b69e2", # Commit ID.
        "tree": "3ade65d8a91eadd009a6c9feea8f87db2c528a53",   # Tree ID.
        "parents": [
          "a477c787fe847ae0482329f69b39ce0fde047359" # Previous commit ID.
        ],
        "author": {
          "name": "...",
          "email": "...",
          "time": "Tue Aug 12 17:17:21 2014"
        },
        "committer": {
          "name": "...",
          "email": "...",
          "time": "Tue Aug 12 17:18:28 2014"
        },
        "message": "...",
        "tree_diff": [...]
      }
    '''
    # Commit information for a branch is obtained by appending '?format=JSON'
    # to the branch URL. Note that '<gitiles_url>/<branch>?format=JSON' is
    # different from '<gitiles_url>/<branch>/?format=JSON': the latter serves
    # the root directory JSON content, whereas the former serves the branch
    # commit info JSON content.
    fetch_future = self._fetcher.FetchAsync(self._base_url + _JSON_FORMAT)
    content_future = self._ResolveFetchContent(self._base_url, fetch_future)
    return content_future.Then(lambda json: _ParseGitilesJson(json)[key])

  def GetCommitID(self):
    '''Returns a future that resolves to the commit ID for this branch.
    '''
    return self._GetCommitInfo('commit')

  def StatAsync(self, path):
    dir_, filename = posixpath.split(path)
    def stat(content):
      stat_info = _CreateStatInfo(content)
      if stat_info.version is None:
        raise FileSystemError('Failed to find version of dir %s' % dir_)
      if IsDirectory(path):
        return stat_info
      if filename not in stat_info.child_versions:
        raise FileNotFoundError(
            '%s from %s was not in child versions for Stat' % (filename, path))
      return StatInfo(stat_info.child_versions[filename])
    fetch_future = self._FetchAsync(ToDirectory(dir_) + _JSON_FORMAT)
    return self._ResolveFetchContent(path, fetch_future).Then(stat)

  def GetIdentity(self):
    # NOTE: Do not use commit information to create the string identity.
    # Doing so will mess up caching.
    if self._commit is None and self._branch != 'master':
      str_id = GITILES_BRANCH_BASE
    else:
      str_id = GITILES_BASE
    return '@'.join((self.__class__.__name__, StringIdentity(str_id)))
