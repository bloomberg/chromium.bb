# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fnmatch import fnmatch
import mimetypes
import os

from file_system import FileNotFoundError
import compiled_file_system as compiled_fs

STATIC_DIR_PREFIX = 'docs'
DOCS_PATH = 'docs'

class ServerInstance(object):
  """This class is used to hold a data source and fetcher for an instance of a
  server. Each new branch will get its own ServerInstance.
  """
  def __init__(self,
               template_data_source_factory,
               example_zipper,
               cache_factory):
    self._template_data_source_factory = template_data_source_factory
    self._example_zipper = example_zipper
    self._cache = cache_factory.Create(lambda _, x: x, compiled_fs.STATIC)
    mimetypes.init()

  def _FetchStaticResource(self, path, response):
    """Fetch a resource in the 'static' directory.
    """
    try:
      result = self._cache.GetFromFile(STATIC_DIR_PREFIX + '/' + path)
    except FileNotFoundError:
      return None
    base, ext = os.path.splitext(path)
    response.headers['content-type'] = mimetypes.types_map[ext]
    return result

  def Get(self, path, request, response):
    # TODO(cduvall): bundle up all the request-scoped data into a single object.
    templates = self._template_data_source_factory.Create(request, path)

    content = None
    if fnmatch(path, 'extensions/examples/*.zip'):
      try:
        content = self._example_zipper.Create(
            path[len('extensions/'):-len('.zip')])
        response.headers['content-type'] = mimetypes.types_map['.zip']
      except FileNotFoundError:
        content = None
    elif path.startswith('extensions/examples/'):
      try:
        content = self._cache.GetFromFile(
            '%s/%s' % (DOCS_PATH, path[len('extensions/'):]))
        response.headers['content-type'] = 'text/plain'
      except FileNotFoundError:
        content = None
    elif path.startswith('static/'):
      content = self._FetchStaticResource(path, response)
    elif path.endswith('.html'):
      content = templates.Render(path)

    response.headers['x-frame-options'] = 'sameorigin'
    if content:
      response.headers['cache-control'] = 'max-age=300'
      response.out.write(content)
    else:
      response.set_status(404);
      response.out.write(templates.Render('404'))
