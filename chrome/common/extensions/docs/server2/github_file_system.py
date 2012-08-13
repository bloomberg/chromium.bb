# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os

import appengine_blobstore as blobstore
import appengine_memcache as memcache
import file_system
from io import BytesIO
from future import Future
from zipfile import ZipFile

ZIP_KEY = 'zipball'

def _MakeKey(version):
  return ZIP_KEY + '.' + str(version)

class _AsyncFetchFutureZip(object):
  def __init__(self, fetcher, blobstore, new_version, old_version):
    self._fetch = fetcher.FetchAsync(ZIP_KEY)
    self._blobstore = blobstore
    self._new_version = new_version
    self._old_version = old_version

  def Get(self):
    blob = self._fetch.Get().content
    self._blobstore.Set(_MakeKey(self._new_version),
                        blob,
                        blobstore.BLOBSTORE_GITHUB)
    self._blobstore.Delete(_MakeKey(self._old_version),
                           blobstore.BLOBSTORE_GITHUB)
    return ZipFile(BytesIO(blob))

class GithubFileSystem(file_system.FileSystem):
  """FileSystem implementation which fetches resources from github.
  """
  def __init__(self, fetcher, memcache, blobstore):
    self._fetcher = fetcher
    self._memcache = memcache
    self._blobstore = blobstore
    self._version = self.Stat(ZIP_KEY).version
    self._GetZip(self._version)

  def _GetZip(self, version):
    blob = self._blobstore.Get(_MakeKey(version), blobstore.BLOBSTORE_GITHUB)
    if blob is not None:
      self._zip_file = Future(value=ZipFile(BytesIO(blob)))
    else:
      self._zip_file = Future(delegate=_AsyncFetchFutureZip(self._fetcher,
                                                            self._blobstore,
                                                            version,
                                                            self._version))
    self._version = version

  def _ReadFile(self, path):
    zip_file = self._zip_file.Get()
    prefix = zip_file.namelist()[0][:-1]
    return zip_file.read(prefix + path)

  def _ListDir(self, path):
    filenames = self._zip_file.Get().namelist()
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

  def Stat(self, path):
    version = self._memcache.Get(path, memcache.MEMCACHE_GITHUB_STAT)
    if version is not None:
      return self.StatInfo(version)
    version = json.loads(
        self._fetcher.Fetch('commits/HEAD').content)['commit']['tree']['sha']
    self._memcache.Set(path, version, memcache.MEMCACHE_GITHUB_STAT)
    return self.StatInfo(version)
