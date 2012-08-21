# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import urlfetch
from future import Future

class _AsyncFetchDelegate(object):
  def __init__(self, rpc):
    self._rpc = rpc

  def Get(self):
    return self._rpc.get_result()

class AppEngineUrlFetcher(object):
  """A wrapper around the App Engine urlfetch module that allows for easy
  async fetches.
  """
  def __init__(self, base_path):
    self._base_path = base_path

  def Fetch(self, url):
    """Fetches a file synchronously.
    """
    if self._base_path is not None:
      return urlfetch.fetch(self._base_path + '/' + url)
    else:
      return urlfetch.fetch(url)

  def FetchAsync(self, url):
    """Fetches a file asynchronously, and returns a Future with the result.
    """
    rpc = urlfetch.create_rpc()
    if self._base_path is not None:
      urlfetch.make_fetch_call(rpc, self._base_path + '/' + url)
    else:
      urlfetch.make_fetch_call(rpc, url)
    return Future(delegate=_AsyncFetchDelegate(rpc))
