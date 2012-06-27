# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

STATIC_DIR_PREFIX = 'docs/server2/'

class ServerInstance(object):
  """This class is used to hold a data source and fetcher for an instance of a
  server. Each new branch will get its own ServerInstance.
  """
  def __init__(self, api_data_source, template_data_source, fetcher):
    self._api_data_source = api_data_source
    self._template_data_source = template_data_source
    self._fetcher = fetcher

  def _NotFound(self, request_handler):
    # TODO: Actual 404 page.
    request_handler.response.set_status(404);
    request_handler.response.out.write('File not found.')

  def _FetchStaticResource(self, path, request_handler):
    """Fetch a resource in the 'static' directory.
    """
    try:
      result = self._fetcher.FetchResource(STATIC_DIR_PREFIX + path)
      for key in result.headers:
        request_handler.response.headers[key] = result.headers[key]
      request_handler.response.out.write(result.content)
    except:
      self._NotFound(request_handler)

  def Get(self, path, request_handler):
    parts = path.rsplit('/', 1)
    filename = parts[-1]
    if parts[0].startswith('static'):
      self._FetchStaticResource(path, request_handler)
      return
    content = self._template_data_source.Render(filename,
                                                self._api_data_source[filename])
    if not content:
      self._NotFound(request_handler)
      return
    request_handler.response.out.write(content)
