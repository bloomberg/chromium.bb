# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fnmatch import fnmatch
import mimetypes
import os

STATIC_DIR_PREFIX = 'docs/server2'
DOCS_PREFIX = 'docs'

class ServerInstance(object):
  """This class is used to hold a data source and fetcher for an instance of a
  server. Each new branch will get its own ServerInstance.
  """
  def __init__(self, template_data_source, example_zipper, cache_builder):
    self._template_data_source = template_data_source
    self._example_zipper = example_zipper
    self._cache = cache_builder.build(lambda x: x)
    mimetypes.init()

  def _FetchStaticResource(self, path, request_handler):
    """Fetch a resource in the 'static' directory.
    """
    try:
      result = self._cache.getFromFile(STATIC_DIR_PREFIX + '/' + path)
      base, ext = os.path.splitext(path)
      request_handler.response.headers['content-type'] = (
          mimetypes.types_map[ext])
      return result
    except:
      return ''

  def Get(self, path, request_handler):
    if fnmatch(path, 'examples/*.zip'):
      content = self._example_zipper.create(path[:-4])
      request_handler.response.headers['content-type'] = (
          mimetypes.types_map['.zip'])
    elif path.startswith('examples/'):
      content = self._cache.getFromFile(DOCS_PREFIX + '/' + path)
      request_handler.response.headers['content-type'] = 'text/plain'
    elif path.startswith('static/'):
      content = self._FetchStaticResource(path, request_handler)
    else:
      content = self._template_data_source.Render(path)
    if content:
      request_handler.response.out.write(content)
    else:
      request_handler.response.set_status(404);
      request_handler.response.out.write(
          self._template_data_source.Render('404'))
