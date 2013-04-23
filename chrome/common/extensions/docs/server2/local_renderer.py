# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from handler import Handler
from fake_fetchers import ConfigureFakeFetchers
import os
from StringIO import StringIO
import urlparse

class _Request(object):
  def __init__(self, path, headers):
    self.path = path
    self.url = 'http://localhost/%s' % path
    self.headers = headers

class _Response(object):
  def __init__(self):
    self.status = 200
    self.out = StringIO()
    self.headers = {}

  def set_status(self, status):
    self.status = status

class LocalRenderer(object):
  '''Renders pages fetched from the local file system.
  '''
  def __init__(self, base_dir):
    self._base_dir = base_dir.replace(os.sep, '/').rstrip('/')

  def Render(self, path, headers={}, always_online=False):
    '''Renders |path|, returning a tuple of (status, contents, headers).
    '''
    # TODO(kalman): do this via a LocalFileSystem not this fake AppEngine stuff.
    ConfigureFakeFetchers(os.path.join(self._base_dir, 'docs'))
    handler_was_always_online = Handler.ALWAYS_ONLINE
    Handler.ALWAYS_ONLINE = always_online
    try:
      response = _Response()
      url_path = urlparse.urlparse(path.replace(os.sep, '/')).path
      Handler(_Request(url_path, headers), response).get()
      content = response.out.getvalue()
      if isinstance(content, unicode):
        content = content.encode('utf-8', 'replace')
      return (content, response.status, response.headers)
    finally:
      Handler.ALWAYS_ONLINE = handler_was_always_online
