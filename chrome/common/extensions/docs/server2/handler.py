# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import sys

from appengine_wrappers import webapp
from appengine_wrappers import memcache
from appengine_wrappers import urlfetch

from api_data_source import APIDataSource
from api_list_data_source import APIListDataSource
from appengine_blobstore import AppEngineBlobstore
from appengine_memcache import AppEngineMemcache
from appengine_url_fetcher import AppEngineUrlFetcher
from branch_utility import BranchUtility
from example_zipper import ExampleZipper
from file_system_cache import FileSystemCache
from github_file_system import GithubFileSystem
from intro_data_source import IntroDataSource
from local_file_system import LocalFileSystem
from memcache_file_system import MemcacheFileSystem
from samples_data_source import SamplesDataSource
from server_instance import ServerInstance
from subversion_file_system import SubversionFileSystem
from template_data_source import TemplateDataSource
import url_constants

# The branch that the server will default to when no branch is specified in the
# URL. This is necessary because it is not possible to pass flags to the script
# handler.
DEFAULT_BRANCH = 'local'

BRANCH_UTILITY_MEMCACHE = AppEngineMemcache('branch_utility')
BRANCH_UTILITY = BranchUtility(url_constants.OMAHA_PROXY_URL,
                               DEFAULT_BRANCH,
                               AppEngineUrlFetcher(''),
                               BRANCH_UTILITY_MEMCACHE)

GITHUB_FILE_SYSTEM = GithubFileSystem(
    AppEngineUrlFetcher(url_constants.GITHUB_URL),
    AppEngineMemcache('github'),
    AppEngineBlobstore())
GITHUB_CACHE_BUILDER = FileSystemCache.Builder(GITHUB_FILE_SYSTEM)

STATIC_DIR_PREFIX = 'docs/server2'
EXTENSIONS_PATH = 'chrome/common/extensions'
DOCS_PATH = 'docs'
API_PATH = 'api'
INTRO_PATH = DOCS_PATH + '/server2/templates/intros'
ARTICLE_PATH = DOCS_PATH + '/server2/templates/articles'
PUBLIC_TEMPLATE_PATH = DOCS_PATH + '/server2/templates/public'
PRIVATE_TEMPLATE_PATH = DOCS_PATH + '/server2/templates/private'
EXAMPLES_PATH = DOCS_PATH + '/examples'
FULL_EXAMPLES_PATH = DOCS_PATH + '/' + EXAMPLES_PATH

# Global cache of instances because Handler is recreated for every request.
SERVER_INSTANCES = {}

def _GetInstanceForBranch(branch, local_path):
  if branch in SERVER_INSTANCES:
    return SERVER_INSTANCES[branch]
  if branch == 'local':
    file_system = LocalFileSystem(local_path)
  else:
    fetcher = AppEngineUrlFetcher(
        _GetURLFromBranch(branch) + '/' + EXTENSIONS_PATH)
    file_system = MemcacheFileSystem(SubversionFileSystem(fetcher),
                                     AppEngineMemcache(branch))

  cache_builder = FileSystemCache.Builder(file_system)
  api_list_data_source = APIListDataSource(cache_builder,
                                           file_system,
                                           API_PATH,
                                           PUBLIC_TEMPLATE_PATH)
  intro_data_source = IntroDataSource(cache_builder,
                                      [INTRO_PATH, ARTICLE_PATH])
  samples_data_source_factory = SamplesDataSource.Factory(branch,
                                                          file_system,
                                                          GITHUB_FILE_SYSTEM,
                                                          cache_builder,
                                                          GITHUB_CACHE_BUILDER,
                                                          EXAMPLES_PATH)
  api_data_source_factory = APIDataSource.Factory(cache_builder,
                                                  API_PATH,
                                                  samples_data_source_factory)
  template_data_source_factory = TemplateDataSource.Factory(
      branch,
      api_data_source_factory,
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

def _GetURLFromBranch(branch):
    if branch == 'trunk':
      return url_constants.SVN_TRUNK_URL + '/src'
    return url_constants.SVN_BRANCH_URL + '/' + branch + '/src'

class Handler(webapp.RequestHandler):
  def __init__(self, request, response, local_path=EXTENSIONS_PATH):
    self._local_path = local_path
    super(Handler, self).__init__(request, response)

  def _NavigateToPath(self, path):
    channel_name, real_path = BRANCH_UTILITY.SplitChannelNameFromPath(path)
    branch = BRANCH_UTILITY.GetBranchNumberForChannelName(channel_name)
    # TODO: Detect that these are directories and serve index.html out of them.
    if real_path.strip('/') == 'apps':
      real_path = 'apps/index.html'
    if real_path.strip('/') == 'extensions':
      real_path = 'extensions/index.html'
    # TODO: This leaks Server instances when branch bumps.
    _GetInstanceForBranch(branch, self._local_path).Get(real_path,
                                                        self.request,
                                                        self.response)

  def get(self):
    path = self.request.path
    if '_ah/warmup' in path:
      logging.info('Warmup request.')
      if DEFAULT_BRANCH != 'local':
        self._NavigateToPath('trunk/extensions/samples.html')
      self._NavigateToPath('dev/extensions/samples.html')
      self._NavigateToPath('beta/extensions/samples.html')
      self._NavigateToPath('stable/extensions/samples.html')
      # Only do this request if we are on the deployed server.
      # Bug: http://crbug.com/141910
      if DEFAULT_BRANCH != 'local':
        self._NavigateToPath('apps/samples.html')
      return

    # Redirect paths like "directory" to "directory/". This is so relative file
    # paths will know to treat this as a directory.
    if os.path.splitext(path)[1] == '' and path[-1] != '/':
      self.redirect(path + '/')
    path = path.replace('/chrome/', '')
    path = path.strip('/')
    self._NavigateToPath(path)
