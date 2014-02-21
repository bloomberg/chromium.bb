# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath
from urlparse import urlsplit

from file_system import FileNotFoundError
from future import Gettable, Future

class Redirector(object):
  def __init__(self, compiled_fs_factory, file_system):
    self._file_system = file_system
    self._cache = compiled_fs_factory.ForJson(file_system)

  def Redirect(self, host, path):
    ''' Check if a path should be redirected, first according to host
    redirection rules, then from rules in redirects.json files.

    Returns the path that should be redirected to, or None if no redirection
    should occur.
    '''
    return self._RedirectOldHosts(host, path) or self._RedirectFromConfig(path)

  def _RedirectFromConfig(self, url):
    ''' Lookup the redirects configuration file in the directory that contains
    the requested resource. If no redirection rule is matched, or no
    configuration file exists, returns None.
    '''
    dirname, filename = posixpath.split(url)

    try:
      rules = self._cache.GetFromFile(
          posixpath.join(dirname, 'redirects.json')).Get()
    except FileNotFoundError:
      return None

    redirect = rules.get(filename)
    if redirect is None:
      return None
    if (redirect.startswith('/') or
        urlsplit(redirect).scheme in ('http', 'https')):
      return redirect

    return posixpath.normpath(posixpath.join(dirname, redirect))

  def _RedirectOldHosts(self, host, path):
    ''' Redirect paths from the old code.google.com to the new
    developer.chrome.com, retaining elements like the channel and https, if
    used.
    '''
    if urlsplit(host).hostname != 'code.google.com':
      return None

    path = path.split('/')
    if path and path[0] == 'chrome':
      path.pop(0)

    return 'https://developer.chrome.com/' + posixpath.join(*path)

  def Cron(self):
    ''' Load files during a cron run.
    '''
    futures = []
    for root, dirs, files in self._file_system.Walk(''):
      if 'redirects.json' in files:
        futures.append(
            self._cache.GetFromFile(posixpath.join(root, 'redirects.json')))
    return Future(delegate=Gettable(lambda: [f.Get() for f in futures]))
