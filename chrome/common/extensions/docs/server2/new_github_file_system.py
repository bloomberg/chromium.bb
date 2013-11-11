# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
from cStringIO import StringIO
import posixpath
import sys
from zipfile import BadZipfile, ZipFile

import appengine_blobstore as blobstore
from appengine_url_fetcher import AppEngineUrlFetcher
from appengine_wrappers import urlfetch
from docs_server_utils import StringIdentity
from file_system import FileNotFoundError, FileSystem, StatInfo
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
    '''Creates a GithubFIleSystem that can be used for testing. It reads zip
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

    self._stat_cache = object_store_creator.Create(
        GithubFileSystem, category='stat-cache')

    # A tuple of the full zip of the repository and a bool indicating whether
    # we've tried to fetch it from datastore yet. Access via self._GetRepoZip().
    self._repo_zip = None, False

  def _GetRepoZip(self):
    data, have_tried_blobstore = self._repo_zip
    if data is not None:
      return data
    if have_tried_blobstore:
      logging.warning('Repo zip queried but it\'s not in datastore. ' +
                      'Have you run Refresh or a cronjob?')
      return None
    # Use |self._repo_url| as the datastore key.
    data = self._blobstore.Get(self._repo_url, _GITHUB_REPOS_NAMESPACE)
    if data is not None:
      data = ZipFile(StringIO(data))
    self._repo_zip = data, True
    return data

  def _SetRepoZip(self, blob):
    self._blobstore.Set(self._repo_url, blob, _GITHUB_REPOS_NAMESPACE)
    self._repo_zip = ZipFile(StringIO(blob)), True

  def _GetNamelist(self):
    '''Returns a list of all file names in a repository zip file.
    '''
    zipfile = self._GetRepoZip()
    if zipfile is None:
      return []
    return zipfile.namelist()

  def _GetVersion(self):
    '''Returns the currently cached version of the repository. The version is a
    'sha' hash value.
    '''
    return self._stat_cache.Get(self._repo_key).Get()

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
      logging.warn('Error parsing JSON from repo %s' % self._repo_url)

  def Refresh(self):
    '''Compares the cached and live stat versions to see if the cached
    repository is out of date. If it is, an async fetch is started and a
    Future is returned. When this Future is evaluated, the fetch will be
    completed and the results cached.

    If no update is needed, None will be returned.
    '''
    version = self._FetchLiveVersion()
    repo_zip_url = self._repo_url + '/zipball'

    def persist_fetch(fetch):
      '''Completes |fetch| and stores the results in blobstore.
      '''
      try:
        blob = fetch.Get().content
      except urlfetch.DownloadError:
        logging.error(
            '%s: Failed to download zip file from repository %s' % repo_zip_url)
      else:
        try:
          zipfile = ZipFile(StringIO(blob))
        except BadZipfile as error:
          logging.error(
              '%s: Bad zip file returned from url %s' % (error, repo_zip_url))
        else:
          self._SetRepoZip(blob)
          self._stat_cache.Set(self._repo_key, version)
      return ()

    # If the cached and live stat versions are different fetch the new repo.
    if version != self._stat_cache.Get('stat').Get():
      logging.info('%s has changed. Fetching.' % self._repo_url)
      fetch = self._fetcher.FetchAsync(
          'zipball', username=self._username, password=self._password)
      return Future(delegate=Gettable(lambda: persist_fetch(fetch)))

    return Future(value=())

  def Read(self, paths, binary=False):
    '''Returns a directory mapping |paths| to the contents of the file at each
    path. If path ends with a '/', it is treated as a directory and is mapped to
    a list of filenames in that directory.

    |binary| is ignored.
    '''
    names = self._GetNamelist()
    if not names:
      # No files in this repository.
      def raise_file_not_found():
        raise FileNotFoundError('No paths can be found, repository is empty')
      return Future(delegate=Gettable(raise_file_not_found))
    else:
      prefix = names[0].split('/')[0]

    reads = {}
    for path in paths:
      full_path = posixpath.join(prefix, path)
      if path == '' or path.endswith('/'):  # If path is a directory...
        trimmed_paths = []
        for f in filter(lambda s: s.startswith(full_path), names):
          if not '/' in f[len(full_path):-1] and not f == full_path:
            trimmed_paths.append(f[len(full_path):])
        reads[path] = trimmed_paths
      else:
        try:
          reads[path] = self._GetRepoZip().read(full_path)
        except KeyError as error:
          return Future(exc_info=(FileNotFoundError,
                                  FileNotFoundError(error),
                                  sys.exc_info()[2]))

    return Future(value=reads)

  def Stat(self, path):
    '''Stats |path| returning its version as as StatInfo object. If |path| ends
    with a '/', it is assumed to be a directory and the StatInfo object returned
    includes child_versions for all paths in the directory.

    File paths do not include the name of the zip file, which is arbitrary and
    useless to consumers.

    Because the repository will only be downloaded once per server version, all
    stat versions are always 0.
    '''
    # Trim off the zip file's name.
    path = path.lstrip('/')
    trimmed = [f.split('/', 1)[1] for f in self._GetNamelist()]

    if path not in trimmed:
      raise FileNotFoundError("No stat found for '%s' in %s" % (path, trimmed))

    version = self._GetVersion()
    child_paths = {}
    if path == '' or path.endswith('/'):
      # Deal with a directory
      for f in filter(lambda s: s.startswith(path), trimmed):
        filename = f[len(path):]
        if not '/' in filename and not f == path:
          child_paths[filename] = StatInfo(version)

    return StatInfo(version, child_paths or None)

  def GetIdentity(self):
    return '%s' % StringIdentity(self.__class__.__name__ + self._repo_key)

  def __repr__(self):
    return '%s(key=%s, url=%s)' % (type(self).__name__,
                                   self._repo_key,
                                   self._repo_url)
