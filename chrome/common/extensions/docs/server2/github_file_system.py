# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

import appengine_blobstore as blobstore
from appengine_wrappers import GetAppVersion, urlfetch
from file_system import FileSystem, StatInfo
from future import Future
from object_store_creator import ObjectStoreCreator
from StringIO import StringIO
from zipfile import ZipFile, BadZipfile

ZIP_KEY = 'zipball'
USERNAME = None
PASSWORD = None

def _MakeBlobstoreKey(version):
  return ZIP_KEY + '.' + str(version)

class _AsyncFetchFutureZip(object):
  def __init__(self,
               fetcher,
               username,
               password,
               blobstore,
               key_to_set,
               key_to_delete=None):
    self._fetcher = fetcher
    self._fetch = fetcher.FetchAsync(ZIP_KEY,
                                     username=username,
                                     password=password)
    self._blobstore = blobstore
    self._key_to_set = key_to_set
    self._key_to_delete = key_to_delete

  def Get(self):
    try:
      result = self._fetch.Get()
      # Check if Github authentication failed.
      if result.status_code == 401:
        logging.error('Github authentication failed for %s, falling back to '
                      'unauthenticated.' % USERNAME)
        blob = self._fetcher.Fetch(ZIP_KEY).content
      else:
        blob = result.content
    except urlfetch.DownloadError as e:
      logging.error('Bad github zip file: %s' % e)
      return None
    if self._key_to_delete is not None:
      self._blobstore.Delete(_MakeBlobstoreKey(self._key_to_delete),
                             blobstore.BLOBSTORE_GITHUB)
    try:
      return_zip = ZipFile(StringIO(blob))
    except BadZipfile as e:
      logging.error('Bad github zip file: %s' % e)
      return None

    self._blobstore.Set(_MakeBlobstoreKey(self._key_to_set),
                        blob,
                        blobstore.BLOBSTORE_GITHUB)
    return return_zip

class GithubFileSystem(FileSystem):
  """FileSystem implementation which fetches resources from github.
  """
  def __init__(self, fetcher, blobstore):
    # The password store is the same for all branches and versions.
    password_store = (ObjectStoreCreator.GlobalFactory()
        .Create(GithubFileSystem).Create(category='password'))
    if USERNAME is None:
      password_data = password_store.GetMulti(('username', 'password')).Get()
      self._username, self._password = (password_data.get('username'),
                                        password_data.get('password'))
    else:
      password_store.SetMulti({'username': USERNAME, 'password': PASSWORD})
      self._username, self._password = (USERNAME, PASSWORD)

    self._fetcher = fetcher
    self._blobstore = blobstore
    self._stat_object_store = (ObjectStoreCreator.SharedFactory(GetAppVersion())
        .Create(GithubFileSystem).Create())
    self._version = None
    self._GetZip(self.Stat(ZIP_KEY).version)

  def _GetZip(self, version):
    blob = self._blobstore.Get(_MakeBlobstoreKey(version),
                               blobstore.BLOBSTORE_GITHUB)
    if blob is not None:
      try:
        self._zip_file = Future(value=ZipFile(StringIO(blob)))
      except BadZipfile as e:
        self._blobstore.Delete(_MakeBlobstoreKey(version),
                               blobstore.BLOBSTORE_GITHUB)
        logging.error('Bad github zip file: %s' % e)
        self._zip_file = Future(value=None)
    else:
      self._zip_file = Future(
          delegate=_AsyncFetchFutureZip(self._fetcher,
                                        self._username,
                                        self._password,
                                        self._blobstore,
                                        version,
                                        key_to_delete=self._version))
    self._version = version

  def _ReadFile(self, path):
    try:
      zip_file = self._zip_file.Get()
    except Exception as e:
      logging.error('Github ReadFile error: %s' % e)
      return ''
    if zip_file is None:
      logging.error('Bad github zip file.')
      return ''
    prefix = zip_file.namelist()[0][:-1]
    return zip_file.read(prefix + path)

  def _ListDir(self, path):
    try:
      zip_file = self._zip_file.Get()
    except Exception as e:
      logging.error('Github ListDir error: %s' % e)
      return []
    if zip_file is None:
      logging.error('Bad github zip file.')
      return []
    filenames = zip_file.namelist()
    # Take out parent directory name (GoogleChrome-chrome-app-samples-c78a30f)
    filenames = [f[len(filenames[0]) - 1:] for f in filenames]
    # Remove the path of the directory we're listing from the filenames.
    filenames = [f[len(path):] for f in filenames
                 if f != path and f.startswith(path)]
    # Remove all files not directly in this directory.
    return [f for f in filenames if f[:-1].count('/') == 0]

  def Read(self, paths, binary=False):
    version = self.Stat(ZIP_KEY).version
    if version != self._version:
      self._GetZip(version)
    result = {}
    for path in paths:
      if path.endswith('/'):
        result[path] = self._ListDir(path)
      else:
        result[path] = self._ReadFile(path)
    return Future(value=result)

  def _DefaultStat(self, path):
    version = 0
    # TODO(kalman): we should replace all of this by wrapping the
    # GithubFileSystem in a CachingFileSystem. A lot of work has been put into
    # CFS to be robust, and GFS is missing out.
    # For example: the following line is wrong, but it could be moot.
    self._stat_object_store.Set(path, version)
    return StatInfo(version)

  def Stat(self, path):
    version = self._stat_object_store.Get(path).Get()
    if version is not None:
      return StatInfo(version)
    try:
      result = self._fetcher.Fetch('commits/HEAD',
                                   username=USERNAME,
                                   password=PASSWORD)
    except urlfetch.DownloadError as e:
      logging.error('GithubFileSystem Stat: %s' % e)
      return self._DefaultStat(path)
    # Check if Github authentication failed.
    if result.status_code == 401:
      logging.error('Github authentication failed for %s, falling back to '
                    'unauthenticated.' % USERNAME)
      try:
        result = self._fetcher.Fetch('commits/HEAD')
      except urlfetch.DownloadError as e:
        logging.error('GithubFileSystem Stat: %s' % e)
        return self._DefaultStat(path)
    version = (json.loads(result.content).get('commit', {})
                                         .get('tree', {})
                                         .get('sha', None))
    # Check if the JSON was valid, and set to 0 if not.
    if version is not None:
      self._stat_object_store.Set(path, version)
    else:
      logging.warning('Problem fetching commit hash from github.')
      return self._DefaultStat(path)
    return StatInfo(version)
