# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import posixpath
import traceback

from branch_utility import BranchUtility
from environment import IsPreviewServer
from file_system import FileNotFoundError
from redirector import Redirector
from servlet import Servlet, Response
from special_paths import SITE_VERIFICATION_FILE
from third_party.handlebar import Handlebar


def _MakeHeaders(content_type):
  return {
    'X-Frame-Options': 'sameorigin',
    'Content-Type': content_type,
    'Cache-Control': 'max-age=300',
  }


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
    path = self._request.path.lstrip('/')

    # The server used to be partitioned based on Chrome channel, but it isn't
    # anymore. Redirect from the old state.
    channel_name, path = BranchUtility.SplitChannelNameFromPath(path)
    if channel_name is not None:
      return Response.Redirect('/' + path, permanent=True)

    server_instance = self._delegate.CreateServerInstance()

    try:
      return self._GetSuccessResponse(path, server_instance)
    except FileNotFoundError:
      # Find the closest 404.html file and serve that, e.g. if the path is
      # extensions/manifest/typo.html then first look for
      # extensions/manifest/404.html, then extensions/404.html, then 404.html.
      #
      # Failing that just print 'Not Found' but that should preferrably never
      # happen, because it would look really bad.
      path_components = path.split('/')
      for i in xrange(len(path_components) - 1, -1, -1):
        try:
          path_404 = posixpath.join(*(path_components[0:i] + ['404']))
          response = self._GetSuccessResponse(path_404, server_instance)
          if response.status != 200:
            continue
          return Response.NotFound(response.content.ToString(),
                                   headers=response.headers)
        except FileNotFoundError: continue
      logging.warning('No 404.html found in %s' % path)
      return Response.NotFound('Not Found', headers=_MakeHeaders('text/plain'))

  def _GetSuccessResponse(self, path, server_instance):
    '''Returns the Response from trying to render |path| with
    |server_instance|.  If |path| isn't found then a FileNotFoundError will be
    raised, such that the only responses that will be returned from this method
    are Ok and Redirect.
    '''
    content_provider, serve_from, path = (
        server_instance.content_providers.GetByServeFrom(path))
    assert content_provider, 'No ContentProvider found for %s' % path

    redirect = Redirector(
        server_instance.compiled_fs_factory,
        content_provider.file_system).Redirect(self._request.host, path)
    if redirect is not None:
      return Response.Redirect(redirect, permanent=False)

    canonical_path = content_provider.GetCanonicalPath(path)
    if canonical_path != path:
      redirect_path = posixpath.join(serve_from, canonical_path)
      return Response.Redirect('/' + redirect_path, permanent=False)

    content_and_type = content_provider.GetContentAndType(path).Get()
    if not content_and_type.content:
      logging.error('%s had empty content' % path)

    content = content_and_type.content
    if isinstance(content, Handlebar):
      template_content, template_warnings = (
          server_instance.template_renderer.Render(content, self._request))
      # HACK: the site verification file (google2ed...) doesn't have a title.
      content, doc_warnings = server_instance.document_renderer.Render(
          template_content,
          path,
          render_title=path != SITE_VERIFICATION_FILE)
      warnings = template_warnings + doc_warnings
      if warnings:
        sep = '\n - '
        logging.warning('Rendering %s:%s%s' % (path, sep, sep.join(warnings)))

    content_type = content_and_type.content_type
    if isinstance(content, unicode):
      content = content.encode('utf-8')
      content_type += '; charset=utf-8'

    return Response.Ok(content, headers=_MakeHeaders(content_type))
