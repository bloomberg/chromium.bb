# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64

from appengine_wrappers import urlfetch
from future import Future

class _AsyncFetchDelegate(object):
  def __init__(self, rpc):
    self._rpc = rpc

  def Get(self):
    return self._rpc.get_result()

def _MakeHeaders(username, password):
  headers = { 'Cache-Control': 'max-age=0' }
  if username is not None and password is not None:
    headers['Authorization'] = 'Basic %s' % base64.encodestring(
        '%s:%s' % (username, password))
  return headers

class AppEngineUrlFetcher(object):
  """A wrapper around the App Engine urlfetch module that allows for easy
  async fetches.
  """
  def __init__(self, base_path=None):
    self._base_path = base_path

  def Fetch(self, url, username=None, password=None):
    """Fetches a file synchronously.
    """
    if self._base_path is not None:
      url = '%s/%s' % (self._base_path, url)
    return urlfetch.fetch(url, headers=_MakeHeaders(username, password))

  def FetchAsync(self, url, username=None, password=None):
    """Fetches a file asynchronously, and returns a Future with the result.
    """
    if self._base_path is not None:
      url = '%s/%s' % (self._base_path, url)
    rpc = urlfetch.create_rpc()
    urlfetch.make_fetch_call(rpc, url, headers=_MakeHeaders(username, password))
    return Future(delegate=_AsyncFetchDelegate(rpc))
