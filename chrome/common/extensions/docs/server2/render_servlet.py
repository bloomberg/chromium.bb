# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fnmatch import fnmatch
import logging
import mimetypes
import traceback
from urlparse import urlsplit

from branch_utility import BranchUtility
from file_system import FileNotFoundError
from servlet import Servlet, Response
import svn_constants

def _IsBinaryMimetype(mimetype):
  return any(
    mimetype.startswith(prefix) for prefix in ['audio', 'image', 'video'])

class RenderServlet(Servlet):
  '''Servlet which renders templates.
  '''
  class Delegate(object):
    def CreateServerInstanceForChannel(self, channel):
      raise NotImplementedError()

  def __init__(self, request, delegate, default_channel='stable'):
    Servlet.__init__(self, request)
    self._delegate = delegate
    self._default_channel = default_channel

  def Get(self):
    ''' Render the page for a request.
    '''
    headers = self._request.headers
    channel, path = BranchUtility.SplitChannelNameFromPath(self._request.path)

    if path.split('/')[-1] == 'redirects.json':
      return Response.Ok('')

    if channel == self._default_channel:
      return Response.Redirect('/' + path)
    if channel is None:
      channel = self._default_channel

    server_instance = self._delegate.CreateServerInstanceForChannel(channel)

    redirect = server_instance.redirector.Redirect(self._request.host, path)
    if redirect is not None:
      if (channel != self._default_channel and
          not urlsplit(redirect).scheme in ('http', 'https')):
        redirect = '/%s%s' % (channel, redirect)
      return Response.Redirect(redirect)

    canonical_path = server_instance.path_canonicalizer.Canonicalize(path)
    redirect = canonical_path.lstrip('/')
    if path != redirect:
      if channel is not None:
        redirect = '%s/%s' % (channel, canonical_path)
      return Response.Redirect('/' + redirect)

    templates = server_instance.template_data_source_factory.Create(
        self._request, path)

    content = None
    content_type = None

    try:
      # At this point, any valid paths ending with '/' have been redirected.
      # Therefore, the response should be a 404 Not Found.
      if path.endswith('/'):
        pass
      elif fnmatch(path, 'extensions/examples/*.zip'):
        content = server_instance.example_zipper.Create(
            path[len('extensions/'):-len('.zip')])
        content_type = 'application/zip'
      elif path.startswith('extensions/examples/'):
        mimetype = mimetypes.guess_type(path)[0] or 'text/plain'
        content = server_instance.content_cache.GetFromFile(
            '%s/%s' % (svn_constants.DOCS_PATH, path[len('extensions/'):]),
            binary=_IsBinaryMimetype(mimetype))
        content_type = mimetype
      elif path.startswith('static/'):
        mimetype = mimetypes.guess_type(path)[0] or 'text/plain'
        content = server_instance.content_cache.GetFromFile(
            ('%s/%s' % (svn_constants.DOCS_PATH, path)),
            binary=_IsBinaryMimetype(mimetype))
        content_type = mimetype
      elif path.endswith('.html'):
        content = templates.Render(path)
        content_type = 'text/html'
    except FileNotFoundError:
      logging.warning(traceback.format_exc())
      content = None

    headers = {'x-frame-options': 'sameorigin'}
    if content is None:
      doc_class = path.split('/', 1)[0]
      content = templates.Render('%s/404' % doc_class)
      if not content:
        content = templates.Render('extensions/404')
      return Response.NotFound(content, headers=headers)

    if not content:
      logging.error('%s had empty content' % path)

    headers.update({
      'content-type': content_type,
      'cache-control': 'max-age=300',
    })
    return Response.Ok(content, headers=headers)
