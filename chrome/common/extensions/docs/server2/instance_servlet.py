# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import IsDevServer
from branch_utility import BranchUtility
from compiled_file_system import CompiledFileSystem
from empty_dir_file_system import EmptyDirFileSystem
from github_file_system import GithubFileSystem
from host_file_system_creator import HostFileSystemCreator
from third_party.json_schema_compiler.memoize import memoize
from render_servlet import RenderServlet
from object_store_creator import ObjectStoreCreator
from server_instance import ServerInstance

class OfflineRenderServletDelegate(RenderServlet.Delegate):
  '''AppEngine instances should never need to call out to SVN. That should only
  ever be done by the cronjobs, which then write the result into DataStore,
  which is as far as instances look. To enable this, crons can pass a custom
  (presumably online) ServerInstance into Get().

  Why? SVN is slow and a bit flaky. Cronjobs failing is annoying but temporary.
  Instances failing affects users, and is really bad.

  Anyway - to enforce this, we actually don't give instances access to SVN.  If
  anything is missing from datastore, it'll be a 404. If the cronjobs don't
  manage to catch everything - uhoh. On the other hand, we'll figure it out
  pretty soon, and it also means that legitimate 404s are caught before a round
  trip to SVN.
  '''
  def __init__(self, delegate):
    self._delegate = delegate

  @memoize
  def CreateServerInstance(self):
    object_store_creator = ObjectStoreCreator(start_empty=False)
    branch_utility = self._delegate.CreateBranchUtility(object_store_creator)
    host_file_system_creator = self._delegate.CreateHostFileSystemCreator(
        object_store_creator)
    host_file_system = host_file_system_creator.Create()
    app_samples_file_system = self._delegate.CreateAppSamplesFileSystem(
        object_store_creator)
    compiled_host_fs_factory = CompiledFileSystem.Factory(
        host_file_system,
        object_store_creator)
    return ServerInstance(object_store_creator,
                          host_file_system,
                          app_samples_file_system,
                          '',
                          compiled_host_fs_factory,
                          branch_utility,
                          host_file_system_creator)

class InstanceServlet(object):
  '''Servlet for running on normal AppEngine instances.
  Create this via GetConstructor() so that cache state can be shared amongst
  them via the memoizing Delegate.
  '''
  class Delegate(object):
    '''Allow runtime dependencies to be overriden for testing.
    '''
    def CreateBranchUtility(self, object_store_creator):
      return BranchUtility.Create(object_store_creator)

    def CreateHostFileSystemCreator(self, object_store_creator):
      return HostFileSystemCreator(object_store_creator, offline=True)

    def CreateAppSamplesFileSystem(self, object_store_creator):
      # TODO(kalman): OfflineServerInstance wrapper for GithubFileSystem, but
      # the cron job doesn't crawl the samples yet.
      return (EmptyDirFileSystem() if IsDevServer() else
              GithubFileSystem.Create(object_store_creator))

  @staticmethod
  def GetConstructor(delegate_for_test=None):
    render_servlet_delegate = OfflineRenderServletDelegate(
        delegate_for_test or InstanceServlet.Delegate())
    return lambda request: RenderServlet(request, render_servlet_delegate)

  # NOTE: if this were a real Servlet it would implement a Get() method, but
  # GetConstructor returns an appropriate lambda function (Request -> Servlet).
