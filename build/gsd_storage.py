#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Provide Google Storage access.

Provide an high-level interface to Google Storage.
Operations are provided to read/write whole files and to
read/write strings. This allows google storage to be treated
more or less like a key+value data-store.
"""


import logging
import os
import shutil
import subprocess
import tempfile

import file_tools
import http_download


GS_PATTERN = 'gs://%s'
GS_HTTPS_PATTERN = 'https://commondatastorage.googleapis.com/%s'


def HttpDownload(url, target):
  """Default download route."""
  http_download.HttpDownload(url, os.path.abspath(target),
                             logger=logging.info)


class GSDStorageError(Exception):
  """Error indicating writing to storage failed."""
  pass


class GSDStorage(object):
  """A wrapper for reading and writing to GSD buckets.

  Multiple read buckets may be specified, and the wrapper will sequentially try
  each and fall back to the next if the previous fails.
  Writing is to a single bucket.
  """
  def __init__(self,
               write_bucket,
               read_buckets,
               gsutil=None,
               call=subprocess.call,
               download=HttpDownload):
    """Init for this class.

    Args:
      write_bucket: Google storage location to write to.
      read_buckets: Google storage locations to read from in preferred order.
      gsutil: List of cmd components needed to call gsutil.
      call: Testing hook to intercept command invocation.
      download: Testing hook to intercept download.
    """
    if gsutil is None:
      try:
        gsutil = [file_tools.Which(os.environ.get('GSUTIL', 'gsutil'))]
      except file_tools.ExecutableNotFound:
        gsutil = ['gsutil']
    assert isinstance(gsutil, list)
    assert isinstance(read_buckets, list)
    self._gsutil = gsutil
    self._write_bucket = write_bucket
    self._read_buckets = read_buckets
    self._call = call
    self._download = download

  def PutFile(self, path, key):
    """Write a file to global storage.

    Args:
      path: Path of the file to write.
      key: Key to store file under.
    Raises:
      GSDStorageError if the underlying storage fails.
    Returns:
      URL written to.
    """
    if self._write_bucket is None:
      raise GSDStorageError('no bucket when storing %s to %s' % (path, key))
    obj = self._write_bucket + '/' + key
    # Using file://c:/foo/bar form of path as gsutil does not like drive
    # letters without it.
    cmd = self._gsutil + [
        'cp', '-a', 'public-read',
        'file://' + os.path.abspath(path).replace(os.sep, '/'),
        GS_PATTERN % obj]
    logging.info('Running: %s' % str(cmd))
    if self._call(cmd) != 0:
      raise GSDStorageError('failed when storing %s to %s (%s)' % (
        path, key, cmd))
    return GS_HTTPS_PATTERN % obj

  def PutData(self, data, key):
    """Write data to global storage.

    Args:
      data: Data to store.
      key: Key to store file under.
    Raises:
      GSDStorageError if the underlying storage fails.
    Returns:
      URL written to.
    """
    handle, path = tempfile.mkstemp(prefix='gdstore', suffix='.tmp')
    try:
      os.close(handle)
      file_tools.WriteFile(data, path)
      return self.PutFile(path, key)
    finally:
      os.remove(path)

  def GetFile(self, key, path):
    """Read a file from global storage.

    Args:
      key: Key to store file under.
      path: Destination filename.
    Returns:
      URL used on success or None for failure.
    """
    for bucket in self._read_buckets:
      try:
        obj = bucket + '/' + key
        uri = GS_HTTPS_PATTERN % obj
        logging.debug('Downloading: %s to %s' % (uri, path))
        self._download(uri, path)
        return uri
      except:
        logging.debug('Failed downloading: %s to %s' % (uri, path))
    return None

  def GetData(self, key):
    """Read data from global storage.

    Args:
      key: Key to store file under.
    Returns:
      Data from storage, or None for failure.
    """
    work_dir = tempfile.mkdtemp(prefix='gdstore', suffix='.tmp')
    try:
      path = os.path.join(work_dir, 'data')
      if self.GetFile(key, path) is not None:
        return file_tools.ReadFile(path)
      return None
    finally:
      shutil.rmtree(work_dir)
