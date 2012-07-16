#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os

# Add the original server location to sys.path so we are able to import
# modules from there.
SERVER_PATH = 'chrome/common/extensions/docs/server2'
if os.path.abspath(SERVER_PATH) not in sys.path:
  sys.path.append(os.path.abspath(SERVER_PATH))

import branch_utility
import urlfetch

from google.appengine.ext import webapp
from google.appengine.ext.webapp.util import run_wsgi_app

from api_data_source import APIDataSource
from fetcher_cache import FetcherCache
from intro_data_source import IntroDataSource
from local_fetcher import LocalFetcher
from samples_data_source import SamplesDataSource
from server_instance import ServerInstance
from subversion_fetcher import SubversionFetcher
from template_data_source import TemplateDataSource
from example_zipper import ExampleZipper

EXTENSIONS_PATH = 'chrome/common/extensions'
DOCS_PATH = 'docs'
API_PATH = 'api'
INTRO_PATH = DOCS_PATH + '/server2/templates/intros'
ARTICLE_PATH = DOCS_PATH + '/server2/templates/articles'
PUBLIC_TEMPLATE_PATH = DOCS_PATH + '/server2/templates/public'
PRIVATE_TEMPLATE_PATH = DOCS_PATH + '/server2/templates/private'
EXAMPLES_PATH = 'examples'
FULL_EXAMPLES_PATH = DOCS_PATH + '/' + EXAMPLES_PATH

# The branch that the server will default to when no branch is specified in the
# URL. This is necessary because it is not possible to pass flags to the script
# handler.
DEFAULT_BRANCH = 'local'

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
    cache_builder = FetcherCache.Builder(fetcher, cache_timeout_seconds)
    api_data_source = APIDataSource(cache_builder, API_PATH)
    intro_data_source = IntroDataSource(cache_builder,
                                        [INTRO_PATH, ARTICLE_PATH])
    samples_data_source = SamplesDataSource(fetcher,
                                            cache_builder,
                                            EXAMPLES_PATH)
    template_data_source = TemplateDataSource(
        branch,
        api_data_source,
        intro_data_source,
        samples_data_source,
        cache_builder,
        [PUBLIC_TEMPLATE_PATH, PRIVATE_TEMPLATE_PATH])
    example_zipper = ExampleZipper(fetcher,
                                   cache_builder,
                                   DOCS_PATH,
                                   EXAMPLES_PATH)
    SERVER_INSTANCES[branch] = ServerInstance(
        template_data_source,
        example_zipper,
        cache_builder)
    return SERVER_INSTANCES[branch]

  def _HandleRequest(self, path):
    channel_name, real_path = (
        branch_utility.SplitChannelNameFromPath(path, default=DEFAULT_BRANCH))
    branch = branch_utility.GetBranchNumberForChannelName(channel_name,
                                                          urlfetch)
    if real_path == '':
      real_path = 'index.html'
    self._GetInstanceForBranch(branch).Get(real_path, self)

  def get(self):
    path = self.request.path
    # Redirect paths like "directory" to "directory/". This is so relative file
    # paths will know to treat this as a directory.
    if os.path.splitext(path)[1] == '' and path[-1] != '/':
      self.redirect(path + '/')
    path = path.replace('/chrome/extensions/', '')
    path = path.strip('/')
    self._HandleRequest(path)

def main():
  handlers = [
    ('/.*', Server),
  ]
  run_wsgi_app(webapp.WSGIApplication(handlers, debug=False))


if __name__ == '__main__':
  main()
