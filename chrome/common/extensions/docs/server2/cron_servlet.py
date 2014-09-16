# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time
import traceback

from app_yaml_helper import AppYamlHelper
from appengine_wrappers import IsDeadlineExceededError, logservice, taskqueue
from branch_utility import BranchUtility
from compiled_file_system import CompiledFileSystem
from custom_logger import CustomLogger
from data_source_registry import CreateDataSources
from environment import GetAppVersion
from gcs_file_system_provider import CloudStorageFileSystemProvider
from github_file_system_provider import GithubFileSystemProvider
from host_file_system_provider import HostFileSystemProvider
from object_store_creator import ObjectStoreCreator
from render_refresher import RenderRefresher
from server_instance import ServerInstance
from servlet import Servlet, Request, Response
from timer import Timer


_log = CustomLogger('cron')


class CronServlet(Servlet):
  '''Servlet which runs a cron job.
  '''
  def __init__(self, request, delegate_for_test=None):
    Servlet.__init__(self, request)
    self._delegate = delegate_for_test or CronServlet.Delegate()

  class Delegate(object):
    '''CronServlet's runtime dependencies. Override for testing.
    '''
    def CreateBranchUtility(self, object_store_creator):
      return BranchUtility.Create(object_store_creator)

    def CreateHostFileSystemProvider(self,
                                     object_store_creator,
                                     pinned_commit=None):
      return HostFileSystemProvider(object_store_creator,
                                    pinned_commit=pinned_commit)

    def CreateGithubFileSystemProvider(self, object_store_creator):
      return GithubFileSystemProvider(object_store_creator)

    def CreateGCSFileSystemProvider(self, object_store_creator):
      return CloudStorageFileSystemProvider(object_store_creator)

    def GetAppVersion(self):
      return GetAppVersion()

  def Get(self):
    # Refreshes may time out, and if they do we need to make sure to flush the
    # logs before the process gets killed (Python gives us a couple of
    # seconds).
    #
    # So, manually flush logs at the end of the cron run. However, sometimes
    # even that isn't enough, which is why in this file we use _log and
    # make it flush the log every time its used.
    logservice.AUTOFLUSH_ENABLED = False
    try:
      return self._GetImpl()
    except BaseException:
      _log.error('Caught top-level exception! %s', traceback.format_exc())
    finally:
      logservice.flush()

  def _GetImpl(self):
    # Cron strategy:
    #
    # Collect all DataSources, the PlatformBundle, the ContentProviders, and
    # any other statically renderered contents (e.g. examples content),
    # and spin up taskqueue tasks which will refresh any cached data relevant
    # to these assets.
    #
    # TODO(rockot/kalman): At the moment examples are not actually refreshed
    # because they're too slow.

    _log.info('starting')

    server_instance = self._GetSafeServerInstance()
    master_fs = server_instance.host_file_system_provider.GetMaster()
    master_commit = master_fs.GetCommitID().Get()

    # This is the guy that would be responsible for refreshing the cache of
    # examples. Here for posterity, hopefully it will be added to the targets
    # below someday.
    render_refresher = RenderRefresher(server_instance, self._request)

    # Get the default taskqueue
    queue = taskqueue.Queue()

    # GAE documentation specifies that it's bad to add tasks to a queue
    # within one second of purging. We wait 2 seconds, because we like
    # to go the extra mile.
    queue.purge()
    time.sleep(2)

    success = True
    try:
      data_sources = CreateDataSources(server_instance)
      targets = (data_sources.items() +
                 [('content_providers', server_instance.content_providers),
                  ('platform_bundle', server_instance.platform_bundle)])
      title = 'initializing %s parallel targets' % len(targets)
      _log.info(title)
      timer = Timer()
      for name, target in targets:
        refresh_paths = target.GetRefreshPaths()
        for path in refresh_paths:
          queue.add(taskqueue.Task(url='/_refresh/%s/%s' % (name, path),
                                   params={'commit': master_commit}))
      _log.info('%s took %s' % (title, timer.Stop().FormatElapsed()))
    except:
      # This should never actually happen (each cron step does its own
      # conservative error checking), so re-raise no matter what it is.
      _log.error('uncaught error: %s' % traceback.format_exc())
      success = False
      raise
    finally:
      _log.info('finished (%s)', 'success' if success else 'FAILED')
      return (Response.Ok('Success') if success else
              Response.InternalError('Failure'))

  def _GetSafeServerInstance(self):
    '''Returns a ServerInstance with a host file system at a safe commit,
    meaning the last commit that the current running version of the server
    existed.
    '''
    delegate = self._delegate

    # IMPORTANT: Get a ServerInstance pinned to the most recent commit, not
    # HEAD. These cron jobs take a while and run very frequently such that
    # there is usually one running at any given time, and eventually a file
    # that we're dealing with will change underneath it, putting the server in
    # an undefined state.
    server_instance_near_head = self._CreateServerInstance(
        self._GetMostRecentCommit())

    app_yaml_handler = AppYamlHelper(
        server_instance_near_head.object_store_creator,
        server_instance_near_head.host_file_system_provider)

    if app_yaml_handler.IsUpToDate(delegate.GetAppVersion()):
      return server_instance_near_head

    # The version in app.yaml is greater than the currently running app's.
    # The safe version is the one before it changed.
    safe_revision = app_yaml_handler.GetFirstRevisionGreaterThan(
        delegate.GetAppVersion()) - 1

    _log.info('app version %s is out of date, safe is %s',
        delegate.GetAppVersion(), safe_revision)

    return self._CreateServerInstance(safe_revision)

  def _GetMostRecentCommit(self):
    '''Gets the commit of the most recent patch submitted to the host file
    system. This is similar to HEAD but it's a concrete commit so won't
    change as the cron runs.
    '''
    head_fs = (
        self._CreateServerInstance(None).host_file_system_provider.GetMaster())
    return head_fs.GetCommitID().Get()

  def _CreateServerInstance(self, commit):
    '''Creates a ServerInstance pinned to |commit|, or HEAD if None.
    NOTE: If passed None it's likely that during the cron run patches will be
    submitted at HEAD, which may change data underneath the cron run.
    '''
    object_store_creator = ObjectStoreCreator(start_empty=True)
    branch_utility = self._delegate.CreateBranchUtility(object_store_creator)
    host_file_system_provider = self._delegate.CreateHostFileSystemProvider(
        object_store_creator, pinned_commit=commit)
    github_file_system_provider = self._delegate.CreateGithubFileSystemProvider(
        object_store_creator)
    gcs_file_system_provider = self._delegate.CreateGCSFileSystemProvider(
        object_store_creator)
    return ServerInstance(object_store_creator,
                          CompiledFileSystem.Factory(object_store_creator),
                          branch_utility,
                          host_file_system_provider,
                          github_file_system_provider,
                          gcs_file_system_provider)
