# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from third_party.cloudstorage import cloudstorage_api
from third_party.cloudstorage import common
from third_party.cloudstorage import errors

from docs_server_utils import StringIdentity
from file_system import FileSystem, FileNotFoundError, StatInfo
from future import Gettable, Future

import logging
import traceback

'''See gcs_file_system_provider.py for documentation on using Google Cloud
Storage as a filesystem.
'''
def _ReadFile(filename):
  try:
    with cloudstorage_api.open(filename, 'r') as f:
      return f.read()
  except errors.Error:
    raise FileNotFoundError('Read failed for %s: %s' % (filename,
        traceback.format_exc()))

def _ListDir(dir_name):
  try:
    files = cloudstorage_api.listbucket(dir_name)
    return [os_path.filename for os_path in files]
  except errors.Error:
    raise FileNotFoundError('cloudstorage.listbucket failed for %s: %s' %
                            (dir_name, traceback.format_exc()))

def _CreateStatInfo(bucket, path):
  bucket = '/%s' % bucket
  full_path = '/'.join( (bucket, path.lstrip('/')) )
  try:
    if full_path.endswith('/'):
      child_versions = dict()
      version = 0
      # Fetching stats for all files under full_path, recursively. The
      # listbucket method uses a prefix approach to simulate hierarchy,
      # but calling it without the "delimiter" argument searches for prefix,
      # which means, for directories, everything beneath it.
      for _file in cloudstorage_api.listbucket(full_path):
        if not _file.is_dir:
          # GCS doesn't have metadata for dirs
          child_stat = cloudstorage_api.stat('%s' % _file.filename).st_ctime
          filename = _file.filename[len(bucket)+1:]
          child_versions[filename] = child_stat
          version = max(version, child_stat)
    else:
      child_versions = None
      version = cloudstorage_api.stat(full_path).st_ctime
    return StatInfo(version, child_versions)
  except (TypeError, errors.Error):
    raise FileNotFoundError('cloudstorage.stat failed for %s: %s' % (path,
                            traceback.format_exc()))


class CloudStorageFileSystem(FileSystem):
  '''FileSystem implementation which fetches resources from Google Cloud
  Storage.
  '''
  def __init__(self, bucket, debug_access_token=None, debug_bucket_prefix=None):
    self._bucket = bucket
    if debug_access_token:
      logging.debug('gcs: using debug access token: %s' % debug_access_token)
      common.set_access_token(debug_access_token)
    if debug_bucket_prefix:
      logging.debug('gcs: prefixing all bucket names with %s' %
                    debug_bucket_prefix)
      self._bucket = debug_bucket_prefix + self._bucket

  def Read(self, paths):
    def resolve():
      try:
        result = {}
        for path in paths:
          full_path = '/%s/%s' % (self._bucket, path.lstrip('/'))
          logging.debug('gcs: requested path %s, reading %s' %
                        (path, full_path))
          if path == '' or path.endswith('/'):
            result[path] = _ListDir(full_path)
          else:
            result[path] = _ReadFile(full_path)
        return result
      except errors.AuthorizationError:
        self._warnAboutAuthError()
        raise

    return Future(delegate=Gettable(resolve))

  def Refresh(self):
    return Future(value=())

  def Stat(self, path):
    try:
      return _CreateStatInfo(self._bucket, path)
    except errors.AuthorizationError:
      self._warnAboutAuthError()
      raise

  def GetIdentity(self):
    return '@'.join((self.__class__.__name__, StringIdentity(self._bucket)))

  def __repr__(self):
    return 'LocalFileSystem(%s)' % self._bucket

  def _warnAboutAuthError(self):
    logging.warn(('Authentication error on Cloud Storage. Check if your'
                  ' appengine project has permissions to Read the GCS'
                  ' buckets. If you are running a local appengine server,'
                  ' you need to set an access_token in'
                  ' local_debug/gcs_debug.conf.'
                  ' Remember that this token expires in less than 10'
                  ' minutes, so keep it updated. See'
                  ' gcs_file_system_provider.py for instructions.'));
    logging.debug(traceback.format_exc())
