# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mimetypes
import os

STATIC_DIR_PREFIX = 'docs/server2/'

class ServerInstance(object):
  """This class is used to hold a data source and fetcher for an instance of a
  server. Each new branch will get its own ServerInstance.
  """
  def __init__(self, api_data_source, template_data_source, cache_builder):
    self._api_data_source = api_data_source
    self._template_data_source = template_data_source
    self._cache = cache_builder.build(lambda x: x)
    mimetypes.init()

  def _FetchStaticResource(self, path, request_handler):
    """Fetch a resource in the 'static' directory.
    """
    try:
      result = self._cache.get(STATIC_DIR_PREFIX + path)
      base, ext = os.path.splitext(path)
      request_handler.response.headers['content-type'] = (
          mimetypes.types_map[ext])
      return result
    except:
      return ''

  def Get(self, path, request_handler):
    if path.startswith('static'):
      content = self._FetchStaticResource(path, request_handler)
    else:
      content = self._template_data_source.Render(path,
                                                  self._api_data_source[path])
    if content:
      request_handler.response.out.write(content)
    else:
      # TODO: Actual 404 page.
      request_handler.response.set_status(404);
      request_handler.response.out.write('File not found.')
