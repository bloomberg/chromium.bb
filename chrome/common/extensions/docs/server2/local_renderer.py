# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Request

class _LocalRenderServletDelegate(object):
  def CreateServerInstanceForChannel(self, channel):
    return ServerInstance.ForLocal()

class LocalRenderer(object):
  '''Renders pages fetched from the local file system.
  '''
  @staticmethod
  def Render(path):
    def render_path(path):
      return RenderServlet(Request(path, 'http://localhost', {}),
                           _LocalRenderServletDelegate(),
                           default_channel='trunk').Get()
    response = render_path(path)
    while response.status in [301, 302]:
      redirect = response.headers['Location']
      sys.stderr.write('<!-- Redirected %s to %s -->\n' % (path, redirect))
      response = render_path(redirect)
    return response
