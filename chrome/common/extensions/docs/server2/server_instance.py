# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fnmatch import fnmatch
import logging
import mimetypes
import traceback
import os

from api_data_source import APIDataSource
from api_list_data_source import APIListDataSource
from appengine_blobstore import AppEngineBlobstore
from appengine_url_fetcher import AppEngineUrlFetcher
from appengine_wrappers import GetAppVersion, IsDevServer
from branch_utility import BranchUtility
from caching_file_system import CachingFileSystem
from compiled_file_system import CompiledFileSystem
from empty_dir_file_system import EmptyDirFileSystem
from example_zipper import ExampleZipper
from file_system import FileNotFoundError
from github_file_system import GithubFileSystem
from intro_data_source import IntroDataSource
from local_file_system import LocalFileSystem
from object_store_creator import ObjectStoreCreator
from offline_file_system import OfflineFileSystem
from path_canonicalizer import PathCanonicalizer
from reference_resolver import ReferenceResolver
from samples_data_source import SamplesDataSource
from sidenav_data_source import SidenavDataSource
from subversion_file_system import SubversionFileSystem
import svn_constants
from template_data_source import TemplateDataSource
from third_party.json_schema_compiler.memoize import memoize
from third_party.json_schema_compiler.model import UnixName
import url_constants

def _IsSamplesDisabled():
  return IsDevServer()

class ServerInstance(object):
  # Lazily create so we don't create github file systems unnecessarily in
  # tests.
  branch_utility = None
  github_file_system = None

  @staticmethod
  @memoize
  def GetOrCreateOffline(channel):
    '''Gets/creates a local ServerInstance, meaning that only resources local to
    the server - memcache, object store, etc, are queried. This amounts to not
    setting up the subversion nor github file systems.
    '''
    branch_utility = ServerInstance._GetOrCreateBranchUtility()
    branch = branch_utility.GetBranchNumberForChannelName(channel)
    object_store_creator_factory = ObjectStoreCreator.Factory(GetAppVersion(),
                                                              branch)
    # No svn nor github file systems. Rely on the crons to fill the caches, and
    # for the caches to exist.
    return ServerInstance(
        channel,
        object_store_creator_factory,
        CachingFileSystem(OfflineFileSystem(SubversionFileSystem),
                          object_store_creator_factory,
                          use_existing_values=True),
        # TODO(kalman): convert GithubFileSystem to be wrappable in a
        # CachingFileSystem so that it can be replaced with an
        # OfflineFileSystem. Currently GFS doesn't set the child versions of
        # stat requests so it doesn't.
        ServerInstance._GetOrCreateGithubFileSystem())

  @staticmethod
  def CreateOnline(channel):
    '''Creates/creates an online server instance, meaning that both local and
    subversion/github resources are queried.
    '''
    branch_utility = ServerInstance._GetOrCreateBranchUtility()
    branch = branch_utility.GetBranchNumberForChannelName(channel)

    if branch == 'trunk':
      svn_url = '/'.join((url_constants.SVN_TRUNK_URL,
                          'src',
                          svn_constants.EXTENSIONS_PATH))
    else:
      svn_url = '/'.join((url_constants.SVN_BRANCH_URL,
                          branch,
                          'src',
                          svn_constants.EXTENSIONS_PATH))

    viewvc_url = svn_url.replace(url_constants.SVN_URL,
                                 url_constants.VIEWVC_URL)

    object_store_creator_factory = ObjectStoreCreator.Factory(GetAppVersion(),
                                                              branch)

    svn_file_system = CachingFileSystem(
        SubversionFileSystem(AppEngineUrlFetcher(svn_url),
                             AppEngineUrlFetcher(viewvc_url)),
        object_store_creator_factory)

    return ServerInstance(channel,
                          object_store_creator_factory,
                          svn_file_system,
                          ServerInstance._GetOrCreateGithubFileSystem())

  @staticmethod
  def CreateForTest(file_system):
    return ServerInstance('test',
                          ObjectStoreCreator.TestFactory(),
                          file_system,
                          None)

  @staticmethod
  def _GetOrCreateBranchUtility():
    if ServerInstance.branch_utility is None:
      ServerInstance.branch_utility = BranchUtility(
          url_constants.OMAHA_PROXY_URL,
          AppEngineUrlFetcher())
    return ServerInstance.branch_utility

  @staticmethod
  def _GetOrCreateGithubFileSystem():
    # Initialising github is pointless if samples are disabled, since it's only
    # used for apps samples.
    if ServerInstance.github_file_system is None:
      if _IsSamplesDisabled():
        ServerInstance.github_file_system = EmptyDirFileSystem()
      else:
        ServerInstance.github_file_system = GithubFileSystem(
            AppEngineUrlFetcher(url_constants.GITHUB_URL),
            AppEngineBlobstore())
    return ServerInstance.github_file_system

  def __init__(self,
               channel,
               object_store_creator_factory,
               svn_file_system,
               github_file_system):
    self.svn_file_system = svn_file_system

    self.github_file_system = github_file_system

    self.compiled_fs_factory = CompiledFileSystem.Factory(
        svn_file_system,
        object_store_creator_factory)

    self.api_list_data_source_factory = APIListDataSource.Factory(
        self.compiled_fs_factory,
        svn_constants.API_PATH,
        svn_constants.PUBLIC_TEMPLATE_PATH)

    self.api_data_source_factory = APIDataSource.Factory(
        self.compiled_fs_factory,
        svn_constants.API_PATH)

    self.ref_resolver_factory = ReferenceResolver.Factory(
        self.api_data_source_factory,
        self.api_list_data_source_factory,
        object_store_creator_factory)

    self.api_data_source_factory.SetReferenceResolverFactory(
        self.ref_resolver_factory)

    # Note: samples are super slow in the dev server because it doesn't support
    # async fetch, so disable them. If you actually want to test samples, then
    # good luck, and modify _IsSamplesDisabled at the top.
    if _IsSamplesDisabled():
      svn_fs_for_samples = EmptyDirFileSystem()
    else:
      svn_fs_for_samples = self.svn_file_system
    self.samples_data_source_factory = SamplesDataSource.Factory(
        channel,
        svn_fs_for_samples,
        self.github_file_system,
        self.ref_resolver_factory,
        object_store_creator_factory,
        svn_constants.EXAMPLES_PATH)

    self.api_data_source_factory.SetSamplesDataSourceFactory(
        self.samples_data_source_factory)

    self.intro_data_source_factory = IntroDataSource.Factory(
        self.compiled_fs_factory,
        self.ref_resolver_factory,
        [svn_constants.INTRO_PATH, svn_constants.ARTICLE_PATH])

    self.sidenav_data_source_factory = SidenavDataSource.Factory(
        self.compiled_fs_factory,
        svn_constants.JSON_PATH)

    self.template_data_source_factory = TemplateDataSource.Factory(
        channel,
        self.api_data_source_factory,
        self.api_list_data_source_factory,
        self.intro_data_source_factory,
        self.samples_data_source_factory,
        self.sidenav_data_source_factory,
        self.compiled_fs_factory,
        self.ref_resolver_factory,
        svn_constants.PUBLIC_TEMPLATE_PATH,
        svn_constants.PRIVATE_TEMPLATE_PATH)

    self.example_zipper = ExampleZipper(
        self.compiled_fs_factory,
        svn_constants.DOCS_PATH)

    self.path_canonicalizer = PathCanonicalizer(
        channel,
        self.compiled_fs_factory)

    self.content_cache = self.compiled_fs_factory.CreateIdentity(ServerInstance)
