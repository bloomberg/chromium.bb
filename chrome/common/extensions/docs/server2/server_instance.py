# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

DOCS_PATH = 'docs/'
STATIC_PATH = DOCS_PATH + 'static/'

class ServerInstance(object):
  """This class is used to hold a data source and fetcher for an instance of a
  server. Each new branch will get its own ServerInstance.
  """
  def __init__(self, api_data_source, template_data_source, fetcher):
    self._api_data_source = api_data_source
    self._template_data_source = template_data_source
    self._fetcher = fetcher


  def Run(self, path, request_handler):
    parts = path.split('/')
    filename = parts[-1]
    content = self._template_data_source.Render(filename,
                                                self._api_data_source[filename])
    if not content:
      logging.info('Template not found for: ' + filename)
      try:
        result = self._fetcher.FetchResource(STATIC_PATH + filename)
        for key in result.headers:
          request_handler.response.headers[key] = result.headers[key]
        content = result.content
      except:
        request_handler.response.set_status(404)
        # TODO should be an actual not found page.
        content = 'File not found.'
    logging.info
    request_handler.response.out.write(content)
