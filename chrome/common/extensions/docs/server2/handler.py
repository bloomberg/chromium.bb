# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import webapp
from cron_servlet import CronServlet
from render_servlet import RenderServlet
from servlet import Request

_SERVLETS = {
  'cron': CronServlet,
}

class Handler(webapp.RequestHandler):
  def __init__(self, request, response):
    super(Handler, self).__init__(request, response)

  def _RedirectSpecialCases(self, path):
    if not path or path == 'index.html':
      self.redirect('http://developer.google.com/chrome')
      return True

    if path == 'apps.html':
      self.redirect('/apps/about_apps.html')
      return True

    return False

  def _RedirectFromCodeDotGoogleDotCom(self, path):
    if (not self.request.url.startswith(('http://code.google.com',
                                         'https://code.google.com'))):
      return False

    new_url = 'http://developer.chrome.com/'

    # switch to https if necessary
    if (self.request.url.startswith('https')):
      new_url = new_url.replace('http', 'https', 1)

    path = path.split('/')
    if len(path) > 0 and path[0] == 'chrome':
      path.pop(0)
    for channel in BranchUtility.GetAllBranchNames():
      if channel in path:
        position = path.index(channel)
        path.pop(position)
        path.insert(0, channel)
    new_url += '/'.join(path)
    self.redirect(new_url)
    return True

  def get(self):
    path, request, response = (self.request.path.lstrip('/'),
                               self.request,
                               self.response)

    if path in ['favicon.ico', 'robots.txt']:
      response.set_status(404)
      return

    if self._RedirectSpecialCases(path):
      return
    if self._RedirectFromCodeDotGoogleDotCom(path):
      return

    if path.startswith('_'):
      servlet_path = path[1:]
      if servlet_path.find('/') == -1:
        servlet_path += '/'
      servlet_name, servlet_path = servlet_path.split('/', 1)
      servlet = _SERVLETS.get(servlet_name)
      if servlet is None:
        response.out.write('"%s" servlet not found' %  servlet_path)
        response.set_status(404)
        return
    else:
      servlet_path = path
      servlet = RenderServlet

    servlet_response = servlet(Request(servlet_path, request.headers)).Get()

    response.out.write(servlet_response.content.ToString())
    response.headers.update(servlet_response.headers)
    response.status = servlet_response.status
