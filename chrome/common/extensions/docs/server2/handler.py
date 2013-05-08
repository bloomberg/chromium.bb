# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from branch_utility import BranchUtility
from cron_servlet import CronServlet
from instance_servlet import InstanceServlet
from servlet import Servlet, Request, Response

_SERVLETS = {
  'cron': CronServlet,
}
_DEFAULT_SERVLET = InstanceServlet.GetConstructor()

class Handler(Servlet):
  def Get(self):
    path = self._request.path

    if path in ['favicon.ico', 'robots.txt']:
      return Response.NotFound('')

    redirect = self._RedirectSpecialCases()
    if redirect is None:
      redirect = self._RedirectFromCodeDotGoogleDotCom()
    if redirect is not None:
      return redirect

    if path.startswith('_'):
      servlet_path = path[1:]
      if servlet_path.find('/') == -1:
        servlet_path += '/'
      servlet_name, servlet_path = servlet_path.split('/', 1)
      servlet = _SERVLETS.get(servlet_name)
      if servlet is None:
        return Response.NotFound('"%s" servlet not found' %  servlet_path)
    else:
      servlet_path = path
      servlet = _DEFAULT_SERVLET

    return servlet(Request(servlet_path,
                           self._request.host,
                           self._request.headers)).Get()

  def _RedirectSpecialCases(self):
    path = self._request.path
    if not path or path == 'index.html':
      return Response.Redirect('http://developer.google.com/chrome')
    if path == 'apps.html':
      return Response.Redirect('/apps/about_apps.html')
    return None

  def _RedirectFromCodeDotGoogleDotCom(self):
    host, path = (self._request.host, self._request.path)

    if not host in ('http://code.google.com', 'https://code.google.com'):
      return None

    new_host = 'http://developer.chrome.com'

    # switch to https if necessary
    if host.startswith('https'):
      new_host = new_host.replace('http', 'https', 1)

    new_path = path.split('/')
    if len(new_path) > 0 and new_path[0] == 'chrome':
      new_path.pop(0)
    for channel in BranchUtility.GetAllChannelNames():
      if channel in new_path:
        position = new_path.index(channel)
        new_path.pop(position)
        new_path.insert(0, channel)
    return Response.Redirect('/'.join([new_host] + new_path))
