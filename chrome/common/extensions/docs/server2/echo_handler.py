#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

# Add the original server location to sys.path so we are able to import
# modules from there.
SERVER_PATH = 'chrome/common/extensions/docs/server2'
if os.path.abspath(SERVER_PATH) not in sys.path:
  sys.path.append(os.path.abspath(SERVER_PATH))

from google.appengine.ext import webapp
from google.appengine.api import memcache
from google.appengine.ext.webapp.util import run_wsgi_app

from api_data_source import APIDataSource
from api_list_data_source import APIListDataSource
from appengine_memcache import AppEngineMemcache
from branch_utility import BranchUtility
from example_zipper import ExampleZipper
from file_system_cache import FileSystemCache
from intro_data_source import IntroDataSource
from local_file_system import LocalFileSystem
from memcache_file_system import MemcacheFileSystem
from samples_data_source import SamplesDataSource
from server_instance import ServerInstance
from subversion_file_system import SubversionFileSystem
from template_data_source import TemplateDataSource
from appengine_url_fetcher import AppEngineUrlFetcher

SVN_URL = 'http://src.chromium.org/chrome'
TRUNK_URL = SVN_URL + '/trunk'
BRANCH_URL = SVN_URL + '/branches'

EXTENSIONS_PATH = 'chrome/common/extensions'
DOCS_PATH = 'docs'
API_PATH = 'api'
INTRO_PATH = DOCS_PATH + '/server2/templates/intros'
ARTICLE_PATH = DOCS_PATH + '/server2/templates/articles'
PUBLIC_TEMPLATE_PATH = DOCS_PATH + '/server2/templates/public'
PRIVATE_TEMPLATE_PATH = DOCS_PATH + '/server2/templates/private'
EXAMPLES_PATH = DOCS_PATH + '/examples'
FULL_EXAMPLES_PATH = DOCS_PATH + '/' + EXAMPLES_PATH

# The branch that the server will default to when no branch is specified in the
# URL. This is necessary because it is not possible to pass flags to the script
# handler.
DEFAULT_BRANCH = 'local'

# Global cache of instances because Handler is recreated for every request.
SERVER_INSTANCES = {}

OMAHA_PROXY_URL = 'http://omahaproxy.appspot.com/json'
BRANCH_UTILITY = BranchUtility(OMAHA_PROXY_URL,
                               DEFAULT_BRANCH,
                               AppEngineUrlFetcher(''),
                               AppEngineMemcache('branch_utility', memcache))

def _GetURLFromBranch(branch):
    if branch == 'trunk':
      return TRUNK_URL + '/src'
    return BRANCH_URL + '/' + branch + '/src'

class Handler(webapp.RequestHandler):
  def _GetInstanceForBranch(self, branch):
    if branch in SERVER_INSTANCES:
      return SERVER_INSTANCES[branch]
    if branch == 'local':
      file_system = LocalFileSystem(EXTENSIONS_PATH)
    else:
      fetcher = AppEngineUrlFetcher(
          _GetURLFromBranch(branch) + '/' + EXTENSIONS_PATH)
      file_system = MemcacheFileSystem(SubversionFileSystem(fetcher),
                                       AppEngineMemcache(branch, memcache))

    cache_builder = FileSystemCache.Builder(file_system)
    api_data_source = APIDataSource(cache_builder, API_PATH)
    api_list_data_source = APIListDataSource(cache_builder,
                                             file_system,
                                             API_PATH,
                                             PUBLIC_TEMPLATE_PATH)
    intro_data_source = IntroDataSource(cache_builder,
                                        [INTRO_PATH, ARTICLE_PATH])
    samples_data_source_factory = SamplesDataSource.Factory(branch,
                                                            file_system,
                                                            cache_builder,
                                                            EXAMPLES_PATH)
    template_data_source_factory = TemplateDataSource.Factory(
        branch,
        api_data_source,
        api_list_data_source,
        intro_data_source,
        samples_data_source_factory,
        cache_builder,
        PUBLIC_TEMPLATE_PATH,
        PRIVATE_TEMPLATE_PATH)
    example_zipper = ExampleZipper(file_system,
                                   cache_builder,
                                   DOCS_PATH,
                                   EXAMPLES_PATH)
    SERVER_INSTANCES[branch] = ServerInstance(
        template_data_source_factory,
        example_zipper,
        cache_builder)
    return SERVER_INSTANCES[branch]

  def get(self):
    path = self.request.path
    if '_ah/warmup' in path:
      logging.info('Warmup request.')
      self.get('/chrome/extensions/trunk/samples.html')
      self.get('/chrome/extensions/dev/samples.html')
      self.get('/chrome/extensions/beta/samples.html')
      self.get('/chrome/extensions/stable/samples.html')
      return

    # Redirect paths like "directory" to "directory/". This is so relative file
    # paths will know to treat this as a directory.
    if os.path.splitext(path)[1] == '' and path[-1] != '/':
      self.redirect(path + '/')
    path = path.replace('/chrome/extensions/', '')
    path = path.strip('/')

    channel_name, real_path = BRANCH_UTILITY.SplitChannelNameFromPath(path)
    branch = BRANCH_UTILITY.GetBranchNumberForChannelName(channel_name)
    if real_path == '':
      real_path = 'index.html'
    # TODO: This leaks Server instances when branch bumps.
    self._GetInstanceForBranch(branch).Get(real_path,
                                           self.request,
                                           self.response)

def main():
  run_wsgi_app(webapp.WSGIApplication([('/.*', Handler)], debug=False))

if __name__ == '__main__':
  main()
