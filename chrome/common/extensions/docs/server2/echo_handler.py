#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

# Add the original server location to sys.path so we are able to import
# modules from there.
SERVER_PATH = 'chrome/common/extensions/docs/server2/'
if os.path.abspath(SERVER_PATH) not in sys.path:
  sys.path.append(os.path.abspath(SERVER_PATH))

import branch_utility
import logging
import urlfetch

from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app

from local_fetcher import LocalFetcher
from server_instance import ServerInstance
from subversion_fetcher import SubversionFetcher
from template_data_source import TemplateDataSource

EXTENSIONS_PATH = 'chrome/common/extensions/'
DOCS_PATH = 'docs/'
PUBLIC_TEMPLATE_PATH = DOCS_PATH + 'template2/public/'
PRIVATE_TEMPLATE_PATH = DOCS_PATH + 'template2/private/'

# Global cache of instances because the Server is recreated for every request.
SERVER_INSTANCES = {}

class Server(webapp.RequestHandler):
  def _GetInstanceForBranch(self, branch):
    if branch in SERVER_INSTANCES:
      return SERVER_INSTANCES[branch]
    if branch == 'local':
      fetcher = LocalFetcher(EXTENSIONS_PATH)
      # No cache for local doc server.
      cache_timeout_seconds = 0
    else:
      fetcher = SubversionFetcher(branch, EXTENSIONS_PATH, urlfetch)
      cache_timeout_seconds = 300
    data_source = TemplateDataSource(
        fetcher,
        [PUBLIC_TEMPLATE_PATH, PRIVATE_TEMPLATE_PATH],
        cache_timeout_seconds)
    SERVER_INSTANCES[branch] = ServerInstance(data_source, fetcher)
    return SERVER_INSTANCES[branch]

  def _HandleRequest(self, path):
    channel_name = branch_utility.GetChannelNameFromPath(path)
    branch = branch_utility.GetBranchNumberForChannelName(channel_name,
                                                          urlfetch)
    self._GetInstanceForBranch(branch).Run(path, self)

  def get(self):
    path = self.request.path
    path = path.replace('/chrome/extensions/', '')
    if len(path) > 0 and path[0] == '/':
      path = path.strip('/')
    self._HandleRequest(path)

def main():
  handlers = [
    ('/.*', Server),
  ]
  run_wsgi_app(webapp.WSGIApplication(handlers, debug=False))


if __name__ == '__main__':
  main()
