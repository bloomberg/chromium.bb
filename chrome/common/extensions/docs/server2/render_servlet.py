# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fnmatch import fnmatch
import logging
import mimetypes
from urlparse import urlsplit

from data_source_registry import CreateDataSources
from file_system import FileNotFoundError
from servlet import Servlet, Response
from svn_constants import DOCS_PATH, PUBLIC_TEMPLATE_PATH

def _IsBinaryMimetype(mimetype):
  return any(
    mimetype.startswith(prefix) for prefix in ['audio', 'image', 'video'])

class RenderServlet(Servlet):
  '''Servlet which renders templates.
  '''
  class Delegate(object):
    def CreateServerInstance(self):
      raise NotImplementedError(self.__class__)

  def __init__(self, request, delegate):
    Servlet.__init__(self, request)
    self._delegate = delegate

  def Get(self):
    ''' Render the page for a request.
    '''
    # TODO(kalman): a consistent path syntax (even a Path class?) so that we
    # can stop being so conservative with stripping and adding back the '/'s.
    path = self._request.path.lstrip('/')

    if path.split('/')[-1] == 'redirects.json':
      return Response.Ok('')

    server_instance = self._delegate.CreateServerInstance()

    redirect = server_instance.redirector.Redirect(self._request.host, path)
    if redirect is not None:
      return Response.Redirect(redirect)

    canonical_result = server_instance.path_canonicalizer.Canonicalize(path)
    redirect = canonical_result.path.lstrip('/')
    if path != redirect:
      return Response.Redirect('/' + redirect,
                               permanent=canonical_result.permanent)

    trunk_fs = server_instance.host_file_system_provider.GetTrunk()
    template_renderer = server_instance.template_renderer
    template_cache = server_instance.compiled_fs_factory.ForTemplates(trunk_fs)

    content = None
    content_type = None

    try:
      # At this point, any valid paths ending with '/' have been redirected.
      # Therefore, the response should be a 404 Not Found.
      if path.endswith('/'):
        pass
      elif fnmatch(path, 'extensions/examples/*.zip'):
        zip_path = DOCS_PATH + path[len('extensions'):-len('.zip')]
        content = server_instance.directory_zipper.Zip(zip_path).Get()
        content_type = 'application/zip'
      elif path.startswith('extensions/examples/'):
        mimetype = mimetypes.guess_type(path)[0] or 'text/plain'
        content = trunk_fs.ReadSingle(
            '%s/%s' % (DOCS_PATH, path[len('extensions/'):]),
            binary=_IsBinaryMimetype(mimetype)).Get()
        content_type = mimetype
      elif path.startswith('static/'):
        mimetype = mimetypes.guess_type(path)[0] or 'text/plain'
        content = trunk_fs.ReadSingle(
            '%s/%s' % (DOCS_PATH, path),
            binary=_IsBinaryMimetype(mimetype)).Get()
        content_type = mimetype
      elif path.endswith('.html'):
        content = template_renderer.Render(
            template_cache.GetFromFile(
                '%s/%s' % (PUBLIC_TEMPLATE_PATH, path)).Get(),
            self._request)
        content_type = 'text/html'
      else:
        content = None
    except FileNotFoundError:
      content = None

    headers = {'x-frame-options': 'sameorigin'}
    if content is None:
      def render_template_or_none(path):
        try:
          return template_renderer.Render(
              template_cache.GetFromFile(
                  '%s/%s' % (PUBLIC_TEMPLATE_PATH, path)).Get(),
              self._request)
        except FileNotFoundError:
          return None
      content = (render_template_or_none('%s/404.html' %
                                         path.split('/', 1)[0]) or
                 render_template_or_none('extensions/404.html') or
                 'Not found')
      return Response.NotFound(content, headers=headers)

    if not content:
      logging.error('%s had empty content' % path)

    headers.update({
      'content-type': content_type,
      'cache-control': 'max-age=300',
    })
    return Response.Ok(content, headers=headers)
