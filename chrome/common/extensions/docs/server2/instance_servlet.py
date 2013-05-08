# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from appengine_wrappers import IsDevServer
from branch_utility import BranchUtility
from caching_file_system import CachingFileSystem
from github_file_system import GithubFileSystem
from third_party.json_schema_compiler.memoize import memoize
from offline_file_system import OfflineFileSystem
from render_servlet import RenderServlet
from subversion_file_system import SubversionFileSystem
from object_store_creator import ObjectStoreCreator
from server_instance import ServerInstance
from servlet import Servlet

class _OfflineRenderServletDelegate(RenderServlet.Delegate):
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
  def CreateServerInstanceForChannel(self, channel):
    object_store_creator = ObjectStoreCreator(channel, start_empty=False)
    branch = (self._delegate.CreateBranchUtility(object_store_creator)
        .GetBranchForChannel(channel))
    host_file_system = CachingFileSystem(
        OfflineFileSystem(self._delegate.CreateHostFileSystemForBranch(branch)),
        object_store_creator)
    app_samples_file_system = self._delegate.CreateAppSamplesFileSystem(
        object_store_creator)
    return ServerInstance(channel,
                          object_store_creator,
                          host_file_system,
                          app_samples_file_system)

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

    def CreateHostFileSystemForBranch(self, branch):
      return SubversionFileSystem.Create(branch)

    def CreateAppSamplesFileSystem(self, object_store_creator):
      # TODO(kalman): OfflineServerInstance wrapper for GithubFileSystem, but
      # the cron job doesn't crawl the samples yet.
      return (EmptyDirFileSystem() if IsDevServer() else
              GithubFileSystem.Create(object_store_creator))

  @staticmethod
  def GetConstructor(delegate_for_test=None):
    render_servlet_delegate = _OfflineRenderServletDelegate(
        delegate_for_test or InstanceServlet.Delegate())
    return lambda request: RenderServlet(request, render_servlet_delegate)

  # NOTE: if this were a real Servlet it would implement a Get() method, but
  # GetConstructor returns an appropriate lambda function (Request -> Servlet).
