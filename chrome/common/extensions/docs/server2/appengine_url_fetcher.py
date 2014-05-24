# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import posixpath

from appengine_wrappers import urlfetch
from environment import GetAppVersion
from future import Future


def _MakeHeaders(username, password):
  headers = {
    'User-Agent': 'Chromium docserver %s' % GetAppVersion(),
    'Cache-Control': 'max-age=0',
  }
  if username is not None and password is not None:
    headers['Authorization'] = 'Basic %s' % base64.b64encode(
        '%s:%s' % (username, password))
  return headers


class AppEngineUrlFetcher(object):
  """A wrapper around the App Engine urlfetch module that allows for easy
  async fetches.
  """
  def __init__(self, base_path=None):
    assert base_path is None or not base_path.endswith('/'), base_path
    self._base_path = base_path

  def Fetch(self, url, username=None, password=None):
    """Fetches a file synchronously.
    """
    return urlfetch.fetch(self._FromBasePath(url),
                          headers=_MakeHeaders(username, password))

  def FetchAsync(self, url, username=None, password=None):
    """Fetches a file asynchronously, and returns a Future with the result.
    """
    rpc = urlfetch.create_rpc()
    urlfetch.make_fetch_call(rpc,
                             self._FromBasePath(url),
                             headers=_MakeHeaders(username, password))
    return Future(callback=lambda: rpc.get_result())

  def _FromBasePath(self, url):
    assert not url.startswith('/'), url
    if self._base_path is not None:
      url = posixpath.join(self._base_path, url) if url else self._base_path
    return url
