# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from StringIO import StringIO
import sys

from appengine_wrappers import webapp
from appengine_wrappers import memcache
from appengine_wrappers import urlfetch

from api_data_source import APIDataSource
from api_list_data_source import APIListDataSource
from appengine_blobstore import AppEngineBlobstore
from in_memory_object_store import InMemoryObjectStore
from appengine_url_fetcher import AppEngineUrlFetcher
from branch_utility import BranchUtility
from example_zipper import ExampleZipper
from compiled_file_system import CompiledFileSystem
import compiled_file_system as compiled_fs
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
# Production settings:
DEFAULT_BRANCHES = { 'extensions': 'stable', 'apps': 'trunk' }
# Dev settings:
# DEFAULT_BRANCHES = { 'extensions': 'local', 'apps': 'local' }

BRANCH_UTILITY_MEMCACHE = InMemoryObjectStore('branch_utility')
BRANCH_UTILITY = BranchUtility(url_constants.OMAHA_PROXY_URL,
                               DEFAULT_BRANCHES,
                               AppEngineUrlFetcher(None),
                               BRANCH_UTILITY_MEMCACHE)

GITHUB_MEMCACHE = InMemoryObjectStore('github')
GITHUB_FILE_SYSTEM = GithubFileSystem(
    AppEngineUrlFetcher(url_constants.GITHUB_URL),
    GITHUB_MEMCACHE,
    AppEngineBlobstore())
GITHUB_COMPILED_FILE_SYSTEM = CompiledFileSystem.Factory(GITHUB_FILE_SYSTEM,
                                                         GITHUB_MEMCACHE)

STATIC_DIR_PREFIX = 'docs/server2'
EXTENSIONS_PATH = 'chrome/common/extensions'
DOCS_PATH = 'docs'
API_PATH = 'api'
TEMPLATE_PATH = DOCS_PATH + '/server2/templates'
INTRO_PATH = TEMPLATE_PATH + '/intros'
ARTICLE_PATH = TEMPLATE_PATH + '/articles'
PUBLIC_TEMPLATE_PATH = TEMPLATE_PATH + '/public'
PRIVATE_TEMPLATE_PATH = TEMPLATE_PATH + '/private'
EXAMPLES_PATH = DOCS_PATH + '/examples'

# Global cache of instances because Handler is recreated for every request.
SERVER_INSTANCES = {}

def _CreateMemcacheFileSystem(branch, branch_memcache):
  svn_url = _GetURLFromBranch(branch) + '/' + EXTENSIONS_PATH
  stat_fetcher = AppEngineUrlFetcher(
      svn_url.replace(url_constants.SVN_URL, url_constants.VIEWVC_URL))
  fetcher = AppEngineUrlFetcher(svn_url)
  return MemcacheFileSystem(SubversionFileSystem(fetcher, stat_fetcher),
                            branch_memcache)

def _MakeInstanceKey(branch, number):
  return '%s/%s' % (branch, number)

def _GetInstanceForBranch(channel_name, local_path):
  branch = BRANCH_UTILITY.GetBranchNumberForChannelName(channel_name)

  # The key for the server is a tuple of |channel_name| with |branch|, since
  # sometimes stable and beta point to the same branch.
  instance_key = _MakeInstanceKey(channel_name, branch)
  instance = SERVER_INSTANCES.get(instance_key, None)
  if instance is not None:
    return instance

  branch_memcache = InMemoryObjectStore(branch)
  if branch == 'local':
    file_system = LocalFileSystem(local_path)
  else:
    file_system = _CreateMemcacheFileSystem(branch, branch_memcache)

  cache_factory = CompiledFileSystem.Factory(file_system, branch_memcache)
  api_list_data_source_factory = APIListDataSource.Factory(cache_factory,
                                                           file_system,
                                                           API_PATH,
                                                           PUBLIC_TEMPLATE_PATH)
  intro_data_source_factory = IntroDataSource.Factory(
      cache_factory,
      [INTRO_PATH, ARTICLE_PATH])
  samples_data_source_factory = SamplesDataSource.Factory(
      channel_name,
      file_system,
      GITHUB_FILE_SYSTEM,
      cache_factory,
      GITHUB_COMPILED_FILE_SYSTEM,
      EXAMPLES_PATH)
  api_data_source_factory = APIDataSource.Factory(cache_factory,
                                                  API_PATH,
                                                  samples_data_source_factory)
  template_data_source_factory = TemplateDataSource.Factory(
      channel_name,
      api_data_source_factory,
      api_list_data_source_factory,
      intro_data_source_factory,
      samples_data_source_factory,
      cache_factory,
      PUBLIC_TEMPLATE_PATH,
      PRIVATE_TEMPLATE_PATH)
  example_zipper = ExampleZipper(file_system,
                                 cache_factory,
                                 DOCS_PATH)

  instance = ServerInstance(template_data_source_factory,
                            example_zipper,
                            cache_factory)
  SERVER_INSTANCES[instance_key] = instance
  return instance

def _GetURLFromBranch(branch):
    if branch == 'trunk':
      return url_constants.SVN_TRUNK_URL + '/src'
    return url_constants.SVN_BRANCH_URL + '/' + branch + '/src'

def _CleanBranches():
  keys = [_MakeInstanceKey(branch, number)
          for branch, number in BRANCH_UTILITY.GetAllBranchNumbers()]
  for key in SERVER_INSTANCES.keys():
    if key not in keys:
      SERVER_INSTANCES.pop(key)

class _MockResponse(object):
  def __init__(self):
    self.status = 200
    self.out = StringIO()
    self.headers = {}

  def set_status(self, status):
    self.status = status

class _MockRequest(object):
  def __init__(self, path):
    self.headers = {}
    self.path = path

class Handler(webapp.RequestHandler):
  def __init__(self, request, response, local_path=EXTENSIONS_PATH):
    self._local_path = local_path
    super(Handler, self).__init__(request, response)

  def _HandleGet(self, path):
    channel_name, real_path = BRANCH_UTILITY.SplitChannelNameFromPath(path)
    # TODO: Detect that these are directories and serve index.html out of them.
    if real_path.strip('/') == 'apps':
      real_path = 'apps/index.html'
    if real_path.strip('/') == 'extensions':
      real_path = 'extensions/index.html'
    _CleanBranches()
    _GetInstanceForBranch(channel_name, self._local_path).Get(real_path,
                                                              self.request,
                                                              self.response)

  def _Render(self, files, channel):
    original_response = self.response
    for f in files:
      path = channel + f.split(PUBLIC_TEMPLATE_PATH)[-1]
      self.request = _MockRequest(path)
      self.response = _MockResponse()
      try:
        self._HandleGet(path)
      except Exception as e:
        logging.error('Error rendering %s: %s' % (path, str(e)))
    self.response = original_response

  class _ValueHolder(object):
    """Class to allow a value to be changed within a lambda.
    """
    def __init__(self, starting_value):
      self._value = starting_value

    def Set(self, value):
      self._value = value

    def Get(self):
      return self._value

  def _HandleCron(self, path):
    # Cache population strategy:
    #
    # We could list all files in PUBLIC_TEMPLATE_PATH then render them. However,
    # this would be inefficient in the common case where files haven't changed
    # since the last cron.
    #
    # Instead, let the CompiledFileSystem give us clues when to re-render: we
    # use the CFS to check whether the templates, examples, or API folders have
    # been changed. If there has been a change, I will call the compilation
    # function. The same is then done separately with the apps samples page,
    # since it pulls its data from Github.
    channel = path.split('/')[-1]
    branch = BRANCH_UTILITY.GetBranchNumberForChannelName(channel)
    logging.info('Running cron job for %s.' % branch)
    branch_memcache = InMemoryObjectStore(branch)
    file_system = _CreateMemcacheFileSystem(branch, branch_memcache)
    factory = CompiledFileSystem.Factory(file_system, branch_memcache)

    needs_render = self._ValueHolder(False)
    invalidation_cache = factory.Create(lambda _: needs_render.Set(True),
                                        compiled_fs.CRON_INVALIDATION)
    for path in [TEMPLATE_PATH, EXAMPLES_PATH, API_PATH]:
      invalidation_cache.GetFromFile(path + '/')

    if needs_render.Get():
      file_listing_cache = factory.Create(lambda x: x,
                                          compiled_fs.CRON_FILE_LISTING)
      self._Render(file_listing_cache.GetFromFileListing(PUBLIC_TEMPLATE_PATH),
                   channel)
    else:
      # If |needs_render| was True, this page was already rendered, and we don't
      # need to render again.
      github_invalidation_cache = GITHUB_COMPILED_FILE_SYSTEM.Create(
          lambda _: needs_render.Set(True),
          compiled_fs.CRON_GITHUB_INVALIDATION)
      if needs_render.Get():
        self._Render([PUBLIC_TEMPLATE_PATH + '/apps/samples.html'], channel)

    self.response.out.write('Success')

  def get(self):
    path = self.request.path
    if path.startswith('/cron'):
      self._HandleCron(path)
    else:
      # Redirect paths like "directory" to "directory/". This is so relative
      # file paths will know to treat this as a directory.
      if os.path.splitext(path)[1] == '' and path[-1] != '/':
        self.redirect(path + '/')
      path = path.replace('/chrome/', '')
      path = path.strip('/')
      self._HandleGet(path)
