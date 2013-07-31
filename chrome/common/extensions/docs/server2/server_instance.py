# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from api_data_source import APIDataSource
from api_list_data_source import APIListDataSource
from appengine_wrappers import IsDevServer
from availability_finder import AvailabilityFinder
from compiled_file_system import CompiledFileSystem
from empty_dir_file_system import EmptyDirFileSystem
from example_zipper import ExampleZipper
from manifest_data_source import ManifestDataSource
from host_file_system_creator import HostFileSystemCreator
from intro_data_source import IntroDataSource
from object_store_creator import ObjectStoreCreator
from path_canonicalizer import PathCanonicalizer
from redirector import Redirector
from reference_resolver import ReferenceResolver
from samples_data_source import SamplesDataSource
from sidenav_data_source import SidenavDataSource
import svn_constants
from template_data_source import TemplateDataSource
from test_branch_utility import TestBranchUtility
from test_object_store import TestObjectStore

class ServerInstance(object):
  def __init__(self,
               object_store_creator,
               host_file_system,
               app_samples_file_system,
               base_path,
               compiled_fs_factory,
               branch_utility,
               host_file_system_creator):
    self.object_store_creator = object_store_creator

    self.host_file_system = host_file_system

    self.app_samples_file_system = app_samples_file_system

    self.compiled_host_fs_factory = compiled_fs_factory

    self.host_file_system_creator = host_file_system_creator

    self.availability_finder_factory = AvailabilityFinder.Factory(
        object_store_creator,
        self.compiled_host_fs_factory,
        branch_utility,
        host_file_system_creator)

    self.api_list_data_source_factory = APIListDataSource.Factory(
        self.compiled_host_fs_factory,
        self.host_file_system,
        svn_constants.API_PATH,
        svn_constants.PUBLIC_TEMPLATE_PATH)

    self.api_data_source_factory = APIDataSource.Factory(
        self.compiled_host_fs_factory,
        svn_constants.API_PATH,
        self.availability_finder_factory)

    self.ref_resolver_factory = ReferenceResolver.Factory(
        self.api_data_source_factory,
        self.api_list_data_source_factory,
        object_store_creator)

    self.api_data_source_factory.SetReferenceResolverFactory(
        self.ref_resolver_factory)

    # Note: samples are super slow in the dev server because it doesn't support
    # async fetch, so disable them.
    if IsDevServer():
      extension_samples_fs = EmptyDirFileSystem()
    else:
      extension_samples_fs = self.host_file_system
    self.samples_data_source_factory = SamplesDataSource.Factory(
        extension_samples_fs,
        CompiledFileSystem.Factory(extension_samples_fs, object_store_creator),
        self.app_samples_file_system,
        CompiledFileSystem.Factory(self.app_samples_file_system,
                                   object_store_creator),
        self.ref_resolver_factory,
        svn_constants.EXAMPLES_PATH,
        base_path)

    self.api_data_source_factory.SetSamplesDataSourceFactory(
        self.samples_data_source_factory)

    self.intro_data_source_factory = IntroDataSource.Factory(
        self.compiled_host_fs_factory,
        self.ref_resolver_factory,
        [svn_constants.INTRO_PATH, svn_constants.ARTICLE_PATH])

    self.sidenav_data_source_factory = SidenavDataSource.Factory(
        self.compiled_host_fs_factory,
        svn_constants.JSON_PATH)

    self.manifest_data_source = ManifestDataSource(
        self.compiled_host_fs_factory,
        host_file_system,
        '/'.join((svn_constants.JSON_PATH, 'manifest.json')),
        '/'.join((svn_constants.API_PATH, '_manifest_features.json')))

    self.template_data_source_factory = TemplateDataSource.Factory(
        self.api_data_source_factory,
        self.api_list_data_source_factory,
        self.intro_data_source_factory,
        self.samples_data_source_factory,
        self.sidenav_data_source_factory,
        self.compiled_host_fs_factory,
        self.ref_resolver_factory,
        self.manifest_data_source,
        svn_constants.PUBLIC_TEMPLATE_PATH,
        svn_constants.PRIVATE_TEMPLATE_PATH,
        base_path)

    self.api_data_source_factory.SetTemplateDataSource(
        self.template_data_source_factory)

    self.example_zipper = ExampleZipper(
        self.compiled_host_fs_factory,
        self.host_file_system,
        svn_constants.DOCS_PATH)

    self.path_canonicalizer = PathCanonicalizer(self.compiled_host_fs_factory)

    self.redirector = Redirector(
        self.compiled_host_fs_factory,
        self.host_file_system,
        svn_constants.PUBLIC_TEMPLATE_PATH)

  @staticmethod
  def ForTest(file_system):
    object_store_creator = ObjectStoreCreator.ForTest()
    return ServerInstance(object_store_creator,
                          file_system,
                          EmptyDirFileSystem(),
                          '',
                          CompiledFileSystem.Factory(file_system,
                                                     object_store_creator),
                          TestBranchUtility.CreateWithCannedData(),
                          HostFileSystemCreator.ForTest(file_system,
                                                        object_store_creator))

  @staticmethod
  def ForLocal():
    object_store_creator = ObjectStoreCreator(start_empty=False,
                                              store_type=TestObjectStore)
    host_file_system_creator = HostFileSystemCreator.ForLocal(
        object_store_creator)
    trunk_file_system = host_file_system_creator.Create()
    return ServerInstance(
        object_store_creator,
        trunk_file_system,
        EmptyDirFileSystem(),
        '',
        CompiledFileSystem.Factory(trunk_file_system, object_store_creator),
        TestBranchUtility.CreateWithCannedData(),
        host_file_system_creator)
