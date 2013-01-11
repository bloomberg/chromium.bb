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
from known_issues_data_source import KnownIssuesDataSource
from local_file_system import LocalFileSystem
from memcache_file_system import MemcacheFileSystem
from reference_resolver import ReferenceResolver
from samples_data_source import SamplesDataSource
from server_instance import ServerInstance
from sidenav_data_source import SidenavDataSource
from subversion_file_system import SubversionFileSystem
from template_data_source import TemplateDataSource
from third_party.json_schema_compiler.model import UnixName
import url_constants

# The branch that the server will default to when no branch is specified in the
# URL. This is necessary because it is not possible to pass flags to the script
# handler.
# Production settings:
DEFAULT_BRANCHES = { 'extensions': 'stable', 'apps': 'trunk' }
# Dev settings:
# DEFAULT_BRANCHES = { 'extensions': 'local', 'apps': 'local' }

# Increment this version to force the server to reload all pages in the first
# cron job that is run.
_VERSION = 1

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

EXTENSIONS_PATH = 'chrome/common/extensions'
DOCS_PATH = 'docs'
API_PATH = 'api'
TEMPLATE_PATH = DOCS_PATH + '/templates'
INTRO_PATH = TEMPLATE_PATH + '/intros'
ARTICLE_PATH = TEMPLATE_PATH + '/articles'
PUBLIC_TEMPLATE_PATH = TEMPLATE_PATH + '/public'
PRIVATE_TEMPLATE_PATH = TEMPLATE_PATH + '/private'
EXAMPLES_PATH = DOCS_PATH + '/examples'
JSON_PATH = TEMPLATE_PATH + '/json'

# Global cache of instances because Handler is recreated for every request.
SERVER_INSTANCES = {}

def _GetURLFromBranch(branch):
    if branch == 'trunk':
      return url_constants.SVN_TRUNK_URL + '/src'
    return url_constants.SVN_BRANCH_URL + '/' + branch + '/src'

def _SplitFilenameUnix(base_dir, files):
  return [UnixName(os.path.splitext(f.split('/')[-1])[0]) for f in files]

def _CreateMemcacheFileSystem(branch, branch_memcache):
  svn_url = _GetURLFromBranch(branch) + '/' + EXTENSIONS_PATH
  stat_fetcher = AppEngineUrlFetcher(
      svn_url.replace(url_constants.SVN_URL, url_constants.VIEWVC_URL))
  fetcher = AppEngineUrlFetcher(svn_url)
  return MemcacheFileSystem(SubversionFileSystem(fetcher, stat_fetcher),
                            branch_memcache)

APPS_BRANCH = BRANCH_UTILITY.GetBranchNumberForChannelName(
    DEFAULT_BRANCHES['apps'])
APPS_MEMCACHE = InMemoryObjectStore(APPS_BRANCH)
APPS_FILE_SYSTEM = _CreateMemcacheFileSystem(APPS_BRANCH, APPS_MEMCACHE)
APPS_COMPILED_FILE_SYSTEM = CompiledFileSystem.Factory(
    APPS_FILE_SYSTEM,
    APPS_MEMCACHE).Create(_SplitFilenameUnix, compiled_fs.APPS_FS)

EXTENSIONS_BRANCH = BRANCH_UTILITY.GetBranchNumberForChannelName(
    DEFAULT_BRANCHES['extensions'])
EXTENSIONS_MEMCACHE = InMemoryObjectStore(EXTENSIONS_BRANCH)
EXTENSIONS_FILE_SYSTEM = _CreateMemcacheFileSystem(EXTENSIONS_BRANCH,
                                                   EXTENSIONS_MEMCACHE)
EXTENSIONS_COMPILED_FILE_SYSTEM = CompiledFileSystem.Factory(
    EXTENSIONS_FILE_SYSTEM,
    EXTENSIONS_MEMCACHE).Create(_SplitFilenameUnix, compiled_fs.EXTENSIONS_FS)

KNOWN_ISSUES_DATA_SOURCE = KnownIssuesDataSource(
    InMemoryObjectStore('KnownIssues'),
    AppEngineUrlFetcher(None))

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
  api_data_source_factory = APIDataSource.Factory(
      cache_factory,
      API_PATH)

  # Give the ReferenceResolver a memcache, to speed up the lookup of
  # duplicate $refs.
  ref_resolver_factory = ReferenceResolver.Factory(
      api_data_source_factory,
      api_list_data_source_factory,
      branch_memcache)
  api_data_source_factory.SetReferenceResolverFactory(ref_resolver_factory)
  samples_data_source_factory = SamplesDataSource.Factory(
      channel_name,
      file_system,
      GITHUB_FILE_SYSTEM,
      cache_factory,
      GITHUB_COMPILED_FILE_SYSTEM,
      ref_resolver_factory,
      EXAMPLES_PATH)
  api_data_source_factory.SetSamplesDataSourceFactory(
      samples_data_source_factory)
  intro_data_source_factory = IntroDataSource.Factory(
      cache_factory,
      ref_resolver_factory,
      [INTRO_PATH, ARTICLE_PATH])
  sidenav_data_source_factory = SidenavDataSource.Factory(cache_factory,
                                                          JSON_PATH)
  template_data_source_factory = TemplateDataSource.Factory(
      channel_name,
      api_data_source_factory,
      api_list_data_source_factory,
      intro_data_source_factory,
      samples_data_source_factory,
      KNOWN_ISSUES_DATA_SOURCE,
      sidenav_data_source_factory,
      cache_factory,
      ref_resolver_factory,
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
    self.url = 'http://localhost' + path

class Handler(webapp.RequestHandler):
  def __init__(self, request, response, local_path=EXTENSIONS_PATH):
    self._local_path = local_path
    super(Handler, self).__init__(request, response)

  def _HandleGet(self, path):
    channel_name, real_path, default = BRANCH_UTILITY.SplitChannelNameFromPath(
        path)
    # TODO: Detect that these are directories and serve index.html out of them.
    if real_path.strip('/') == 'apps':
      real_path = 'apps/index.html'
    if real_path.strip('/') == 'extensions':
      real_path = 'extensions/index.html'

    if (not real_path.startswith('extensions/') and
        not real_path.startswith('apps/') and
        not real_path.startswith('static/')):
      if self._RedirectBadPaths(real_path, channel_name, default):
        return

    _CleanBranches()
    _GetInstanceForBranch(channel_name, self._local_path).Get(real_path,
                                                              self.request,
                                                              self.response)

  def _Render(self, files, channel):
    original_response = self.response
    for f in files:
      if f.endswith('404.html'):
        continue
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
    # been changed. If there has been a change, the compilation function will
    # be called. The same is then done separately with the apps samples page,
    # since it pulls its data from Github.
    channel = path.split('/')[-1]
    branch = BRANCH_UTILITY.GetBranchNumberForChannelName(channel)
    logging.info('Running cron job for %s.' % branch)
    branch_memcache = InMemoryObjectStore(branch)
    file_system = _CreateMemcacheFileSystem(branch, branch_memcache)
    factory = CompiledFileSystem.Factory(file_system, branch_memcache)

    needs_render = self._ValueHolder(False)
    invalidation_cache = factory.Create(lambda _, __: needs_render.Set(True),
                                        compiled_fs.CRON_INVALIDATION,
                                        version=_VERSION)
    for path in [TEMPLATE_PATH, EXAMPLES_PATH, API_PATH]:
      invalidation_cache.GetFromFile(path + '/')

    if needs_render.Get():
      file_listing_cache = factory.Create(lambda _, x: x,
                                          compiled_fs.CRON_FILE_LISTING)
      self._Render(file_listing_cache.GetFromFileListing(PUBLIC_TEMPLATE_PATH),
                   channel)
    else:
      # If |needs_render| was True, this page was already rendered, and we don't
      # need to render again.
      github_invalidation_cache = GITHUB_COMPILED_FILE_SYSTEM.Create(
          lambda _, __: needs_render.Set(True),
          compiled_fs.CRON_GITHUB_INVALIDATION)
      if needs_render.Get():
        self._Render([PUBLIC_TEMPLATE_PATH + '/apps/samples.html'], channel)

      # It's good to keep the extensions samples page fresh, because if it
      # gets dropped from the cache ALL the extensions pages time out.
      self._Render([PUBLIC_TEMPLATE_PATH + '/extensions/samples.html'], channel)

    self.response.out.write('Success')

  def _RedirectSpecialCases(self, path):
    google_dev_url = 'http://developer.google.com/chrome'
    if path == '/' or path == '/index.html':
      self.redirect(google_dev_url)
      return True

    if path == '/apps.html':
      self.redirect('/apps/about_apps.html')
      return True

    return False

  def _RedirectBadPaths(self, path, channel_name, default):
    if '/' in path or path == '404.html':
      return False
    apps_templates = APPS_COMPILED_FILE_SYSTEM.GetFromFileListing(
        PUBLIC_TEMPLATE_PATH + '/apps')
    extensions_templates = EXTENSIONS_COMPILED_FILE_SYSTEM.GetFromFileListing(
        PUBLIC_TEMPLATE_PATH + '/extensions')
    unix_path = UnixName(os.path.splitext(path)[0])
    if default:
      apps_path = '/apps/%s' % path
      extensions_path = '/extensions/%s' % path
    else:
      apps_path = '/%s/apps/%s' % (channel_name, path)
      extensions_path = '/%s/extensions/%s' % (channel_name, path)
    if unix_path in extensions_templates:
      self.redirect(extensions_path)
    elif unix_path in apps_templates:
      self.redirect(apps_path)
    else:
      self.redirect(extensions_path)
    return True

  def _RedirectFromCodeDotGoogleDotCom(self, path):
    if (not self.request.url.startswith(('http://code.google.com',
                                         'https://code.google.com'))):
      return False

    newUrl = 'http://developer.chrome.com/'

    # switch to https if necessary
    if (self.request.url.startswith('https')):
      newUrl = newUrl.replace('http', 'https', 1)

    path = path.split('/')
    if len(path) > 0 and path[0] == 'chrome':
      path.pop(0)
    for channel in BRANCH_UTILITY.GetAllBranchNames():
      if channel in path:
        position = path.index(channel)
        path.pop(position)
        path.insert(0, channel)
    newUrl += '/'.join(path)
    self.redirect(newUrl)
    return True

  def get(self):
    path = self.request.path
    if self._RedirectSpecialCases(path):
      return

    if path.startswith('/cron'):
      self._HandleCron(path)
      return

    # Redirect paths like "directory" to "directory/". This is so relative
    # file paths will know to treat this as a directory.
    if os.path.splitext(path)[1] == '' and path[-1] != '/':
      self.redirect(path + '/')
      return

    path = path.strip('/')
    if not self._RedirectFromCodeDotGoogleDotCom(path):
      self._HandleGet(path)
