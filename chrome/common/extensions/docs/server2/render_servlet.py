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

_DEFAULT_CHANNEL = 'stable'

_ALWAYS_ONLINE = IsDevServer()

def _IsBinaryMimetype(mimetype):
  return any(mimetype.startswith(prefix)
             for prefix in ['audio', 'image', 'video'])

def AlwaysOnline(fn):
  '''A function decorator which forces the rendering to be always online rather
  than the default offline behaviour. Useful for testing.
  '''
  def impl(*args, **optargs):
    global _ALWAYS_ONLINE
    was_always_online = _ALWAYS_ONLINE
    try:
      _ALWAYS_ONLINE = True
      return fn(*args, **optargs)
    finally:
      _ALWAYS_ONLINE = was_always_online
  return impl

class RenderServlet(Servlet):
  '''Servlet which renders templates.
  '''
  def Get(self, server_instance=None):
    path_with_channel, headers = (self._request.path.lstrip('/'),
                                  self._request.headers)

    # Redirect "extensions" and "extensions/" to "extensions/index.html", etc.
    if (os.path.splitext(path_with_channel)[1] == '' and
        path_with_channel.find('/') == -1):
      path_with_channel += '/'
    if path_with_channel.endswith('/'):
      return Response.Redirect(path_with_channel + 'index.html')

    channel, path = BranchUtility.SplitChannelNameFromPath(path_with_channel)

    if channel == _DEFAULT_CHANNEL:
      return Response.Redirect('/%s' % path)

    if channel is None:
      channel = _DEFAULT_CHANNEL

    # AppEngine instances should never need to call out to SVN. That should
    # only ever be done by the cronjobs, which then write the result into
    # DataStore, which is as far as instances look. To enable this, crons can
    # pass a custom (presumably online) ServerInstance into Get().
    #
    # Why? SVN is slow and a bit flaky. Cronjobs failing is annoying but
    # temporary. Instances failing affects users, and is really bad.
    #
    # Anyway - to enforce this, we actually don't give instances access to SVN.
    # If anything is missing from datastore, it'll be a 404. If the cronjobs
    # don't manage to catch everything - uhoh. On the other hand, we'll figure
    # it out pretty soon, and it also means that legitimate 404s are caught
    # before a round trip to SVN.
    if server_instance is None:
      # The ALWAYS_ONLINE thing is for tests and preview.py that shouldn't need
      # to run the cron before rendering things.
      constructor = (ServerInstance.CreateOnline if _ALWAYS_ONLINE else
                     ServerInstance.GetOrCreateOffline)
      server_instance = constructor(channel)

    canonical_path = server_instance.path_canonicalizer.Canonicalize(path)
    if path != canonical_path:
      return Response.Redirect(canonical_path if channel is None else
                               '%s/%s' % (channel, canonical_path))

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
