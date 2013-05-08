# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fnmatch import fnmatch
import logging
import mimetypes
import os
import traceback

from appengine_wrappers import IsDevServer
from branch_utility import BranchUtility
from file_system import FileNotFoundError
from server_instance import ServerInstance
from servlet import Servlet, Response
import svn_constants

def _IsBinaryMimetype(mimetype):
  return any(mimetype.startswith(prefix)
             for prefix in ['audio', 'image', 'video'])

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
    path_with_channel, headers = (self._request.path, self._request.headers)

    # Redirect "extensions" and "extensions/" to "extensions/index.html", etc.
    if (os.path.splitext(path_with_channel)[1] == '' and
        path_with_channel.find('/') == -1):
      path_with_channel += '/'
    if path_with_channel.endswith('/'):
      return Response.Redirect('/%sindex.html' % path_with_channel)

    channel, path = BranchUtility.SplitChannelNameFromPath(path_with_channel)

    if channel == self._default_channel:
      return Response.Redirect('/%s' % path)

    if channel is None:
      channel = self._default_channel

    server_instance = self._delegate.CreateServerInstanceForChannel(channel)

    canonical_path = (
        server_instance.path_canonicalizer.Canonicalize(path).lstrip('/'))
    if path != canonical_path:
      redirect_path = (canonical_path if channel is None else
                       '%s/%s' % (channel, canonical_path))
      return Response.Redirect('/%s' % redirect_path)

    templates = server_instance.template_data_source_factory.Create(
        self._request, path)

    content = None
    content_type = None

    try:
      if fnmatch(path, 'extensions/examples/*.zip'):
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
    except FileNotFoundError as e:
      logging.warning(traceback.format_exc())
      content = None

    headers = {'x-frame-options': 'sameorigin'}
    if content is None:
      return Response.NotFound(templates.Render('404'), headers=headers)

    if not content:
      logging.error('%s had empty content' % path)

    headers.update({
      'content-type': content_type,
      'cache-control': 'max-age=300',
    })
    return Response.Ok(content, headers=headers)
