# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
from cStringIO import StringIO
import posixpath
import sys
import traceback
from zipfile import BadZipfile, ZipFile

import appengine_blobstore as blobstore
from appengine_url_fetcher import AppEngineUrlFetcher
from appengine_wrappers import urlfetch
from docs_server_utils import StringIdentity
from file_system import FileNotFoundError, FileSystem, FileSystemError, StatInfo
from future import Future, Gettable
from object_store_creator import ObjectStoreCreator
import url_constants


_GITHUB_REPOS_NAMESPACE = 'GithubRepos'


def _LoadCredentials(object_store_creator):
  '''Returns (username, password) from |password_store|.
  '''
  password_store = object_store_creator.Create(
      GithubFileSystem,
      app_version=None,
      category='password',
      start_empty=False)
  password_data = password_store.GetMulti(('username', 'password')).Get()
  return password_data.get('username'), password_data.get('password')


class _GithubZipFile(object):
  '''A view of a ZipFile with a more convenient interface which ignores the
  'zipball' prefix that all paths have. The zip files that come straight from
  GitHub have paths like ['zipball/foo.txt', 'zipball/bar.txt'] but we only
  care about ['foo.txt', 'bar.txt'].
  '''

  @classmethod
  def Create(cls, repo_name, blob):
    try:
      zipball = ZipFile(StringIO(blob))
    except:
      logging.warning('zipball "%s" is not a valid zip' % repo_name)
      return None

    if not zipball.namelist():
      logging.warning('zipball "%s" is empty' % repo_name)
      return None

    name_prefix = None  # probably 'zipball'
    paths = []
    for name in zipball.namelist():
      prefix, path = name.split('/', 1)
      if name_prefix and prefix != name_prefix:
        logging.warning('zipball "%s" has names with inconsistent prefix: %s' %
                        (repo_name, zipball.namelist()))
        return None
      name_prefix = prefix
      paths.append(path)
    return cls(zipball, name_prefix, paths)

  def __init__(self, zipball, name_prefix, paths):
    self._zipball = zipball
    self._name_prefix = name_prefix
    self._paths = paths

  def Paths(self):
    '''Return all file paths in this zip file.
    '''
    return self._paths

  def List(self, path):
    '''Returns all files within a directory at |path|. Not recursive. Paths
    are returned relative to |path|.
    '''
    assert path == '' or path.endswith('/')
    return [p[len(path):] for p in self._paths
            if p != path and
               p.startswith(path) and
               '/' not in p[len(path):].rstrip('/')]

  def Read(self, path):
    '''Returns the contents of |path|. Raises a KeyError if it doesn't exist.
    '''
    return self._zipball.read(posixpath.join(self._name_prefix, path))


class GithubFileSystem(FileSystem):
  '''Allows reading from a github.com repository.
  '''
  @staticmethod
  def Create(owner, repo, object_store_creator):
    '''Creates a GithubFileSystem that corresponds to a single github repository
    specified by |owner| and |repo|.
    '''
    return GithubFileSystem(
        url_constants.GITHUB_REPOS,
        owner,
        repo,
        object_store_creator,
        AppEngineUrlFetcher)

  @staticmethod
  def ForTest(repo, fake_fetcher, path=None, object_store_creator=None):
    '''Creates a GithubFileSystem that can be used for testing. It reads zip
    files and commit data from server2/test_data/github_file_system/test_owner
    instead of github.com. It reads from files specified by |repo|.
    '''
    return GithubFileSystem(
        path if path is not None else 'test_data/github_file_system',
        'test_owner',
        repo,
        object_store_creator or ObjectStoreCreator.ForTest(),
        fake_fetcher)

  def __init__(self, base_url, owner, repo, object_store_creator, Fetcher):
    self._repo_key = '%s/%s' % (owner, repo)
    self._repo_url = '%s/%s/%s' % (base_url, owner, repo)

    self._blobstore = blobstore.AppEngineBlobstore()
    # Lookup the chrome github api credentials.
    self._username, self._password = _LoadCredentials(object_store_creator)
    self._fetcher = Fetcher(self._repo_url)

    # start_empty=False here to maintain the most recent stat across cron runs.
    # Refresh() will always re-stat and use that to decide whether to download
    # the zipball.
    self._stat_cache = object_store_creator.Create(
        GithubFileSystem, category='stat-cache', start_empty=False)

    # A Future to the github zip file. Normally this Future will resolve itself
    # by querying blobstore for the blob; however, Refresh() may decide to
    # override this with a new blob if it's out of date.
    def resolve_from_blobstore():
      blob = self._blobstore.Get(self._repo_url, _GITHUB_REPOS_NAMESPACE)
      return _GithubZipFile.Create(self._repo_key, blob) if blob else None
    self._repo_zip = Future(delegate=Gettable(resolve_from_blobstore))

  def _GetCachedVersion(self):
    '''Returns the currently cached version of the repository. The version is a
    'sha' hash value.
    '''
    return self._stat_cache.Get(self._repo_key).Get()

  def _SetCachedVersion(self, version):
    '''Sets the currently cached version of the repository. The version is a
    'sha' hash value.
    '''
    self._stat_cache.Set(self._repo_key, version)

  def _FetchLiveVersion(self):
    '''Fetches the current repository version from github.com and returns it.
    The version is a 'sha' hash value.
    '''
    # TODO(kalman): Do this asynchronously (use FetchAsync).
    result = self._fetcher.Fetch(
        'commits/HEAD', username=self._username, password=self._password)

    try:
      return json.loads(result.content)['commit']['tree']['sha']
    except (KeyError, ValueError):
      raise FileSystemError('Error parsing JSON from repo %s: %s' %
                            (self._repo_url, traceback.format_exc()))

  def Refresh(self):
    '''Compares the cached and live stat versions to see if the cached
    repository is out of date. If it is, an async fetch is started and a
    Future is returned. When this Future is evaluated, the fetch will be
    completed and the results cached.

    If no update is needed, None will be returned.
    '''
    version = self._FetchLiveVersion()
    if version == self._GetCachedVersion():
      logging.info('%s is up to date.' % self._repo_url)
      # By default this Future will load the blob from datastore.
      return self._repo_zip

    logging.info('%s has changed. Re-fetching.' % self._repo_url)
    fetch = self._fetcher.FetchAsync(
        'zipball', username=self._username, password=self._password)

    def resolve():
      '''Completes |fetch| and stores the results in blobstore.
      '''
      repo_zip_url = self._repo_url + '/zipball'
      try:
        blob = fetch.Get().content
      except urlfetch.DownloadError:
        raise FileSystemError(
            '%s: Failed to download zip file from repository %s' % repo_zip_url)

      repo_zip = _GithubZipFile.Create(self._repo_key, blob)
      if repo_zip is None:
        raise FileSystemError('%s: failed to create zip' % repo_zip_url)

      # Success. Update blobstore and the version.
      self._blobstore.Set(self._repo_url, blob, _GITHUB_REPOS_NAMESPACE)
      self._SetCachedVersion(version)
      return repo_zip

    self._repo_zip = Future(delegate=Gettable(resolve))
    return self._repo_zip

  def Read(self, paths, binary=False):
    '''Returns a directory mapping |paths| to the contents of the file at each
    path. If path ends with a '/', it is treated as a directory and is mapped to
    a list of filenames in that directory.

    |binary| is ignored.
    '''
    def resolve():
      repo_zip = self._repo_zip.Get()
      if repo_zip is None:
        raise FileNotFoundError('"%s" has not been Refreshed' % self._repo_key)
      reads = {}
      for path in paths:
        if path not in repo_zip.Paths():
          raise FileNotFoundError('"%s": %s not found' % (self._repo_key, path))
        if path == '' or path.endswith('/'):
          reads[path] = repo_zip.List(path)
        else:
          reads[path] = repo_zip.Read(path)
      return reads

    # Delay reading until after self._repo_zip has finished fetching.
    return Future(delegate=Gettable(resolve))

  def Stat(self, path):
    '''Stats |path| returning its version as as StatInfo object. If |path| ends
    with a '/', it is assumed to be a directory and the StatInfo object returned
    includes child_versions for all paths in the directory.

    File paths do not include the name of the zip file, which is arbitrary and
    useless to consumers.

    Because the repository will only be downloaded once per server version, all
    stat versions are always 0.
    '''
    repo_zip = self._repo_zip.Get()
    if repo_zip is None:
      raise FileNotFoundError('"%s" has never been Refreshed' % self._repo_key)

    if path not in repo_zip.Paths():
      raise FileNotFoundError('"%s" does not contain file "%s"' %
                              (self._repo_key, path))

    version = self._GetCachedVersion()
    assert version, ('There was a zipball in datastore; there should be a '
                     'version cached for it')

    stat_info = StatInfo(version)
    if path == '' or path.endswith('/'):
      stat_info.child_versions = dict((p, StatInfo(version))
                                      for p in repo_zip.List(path))
    return stat_info

  def GetIdentity(self):
    return '%s' % StringIdentity(self.__class__.__name__ + self._repo_key)

  def __repr__(self):
    return '%s(key=%s, url=%s)' % (type(self).__name__,
                                   self._repo_key,
                                   self._repo_url)
