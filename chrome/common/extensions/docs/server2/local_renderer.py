# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import urlparse

from render_servlet import RenderServlet
from fake_fetchers import ConfigureFakeFetchers
from servlet import Request

class LocalRenderer(object):
  '''Renders pages fetched from the local file system.
  '''
  def __init__(self, base_dir):
    self._base_dir = base_dir.replace(os.sep, '/').rstrip('/')

  def Render(self, path, headers=None, servlet=RenderServlet):
    '''Renders |path|, returning a tuple of (status, contents, headers).
    '''
    headers = headers or {}
    # TODO(kalman): do this via a LocalFileSystem not this fake AppEngine stuff.
    ConfigureFakeFetchers(os.path.join(self._base_dir, 'docs'))
    url_path = urlparse.urlparse(path.replace(os.sep, '/')).path
    return servlet(Request(url_path, headers)).Get()
