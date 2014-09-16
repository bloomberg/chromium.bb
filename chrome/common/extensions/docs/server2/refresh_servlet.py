# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import traceback

from app_yaml_helper import AppYamlHelper
from appengine_wrappers import IsDeadlineExceededError, logservice
from branch_utility import BranchUtility
from compiled_file_system import CompiledFileSystem
from custom_logger import CustomLogger
from data_source_registry import CreateDataSource
from environment import GetAppVersion
from file_system import IsFileSystemThrottledError
from future import Future
from gcs_file_system_provider import CloudStorageFileSystemProvider
from github_file_system_provider import GithubFileSystemProvider
from host_file_system_provider import HostFileSystemProvider
from object_store_creator import ObjectStoreCreator
from server_instance import ServerInstance
from servlet import Servlet, Request, Response
from timer import Timer, TimerClosure



_log = CustomLogger('refresh')


class RefreshServlet(Servlet):
  '''Servlet which refreshes a single data source.
  '''
  def __init__(self, request, delegate_for_test=None):
    Servlet.__init__(self, request)
    self._delegate = delegate_for_test or RefreshServlet.Delegate()

  class Delegate(object):
    '''RefreshServlet's runtime dependencies. Override for testing.
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
    # Manually flush logs at the end of the run. However, sometimes
    # even that isn't enough, which is why in this file we use the
    # custom logger and make it flush the log every time its used.
    logservice.AUTOFLUSH_ENABLED = False
    try:
      return self._GetImpl()
    except BaseException:
      _log.error('Caught top-level exception! %s', traceback.format_exc())
    finally:
      logservice.flush()

  def _GetImpl(self):
    path = self._request.path.strip('/')
    parts = self._request.path.split('/', 1)
    source_name = parts[0]
    if len(parts) == 2:
      source_path = parts[1]
    else:
      source_path = None

    _log.info('starting refresh of %s DataSource %s' %
        (source_name, '' if source_path is None else '[%s]' % source_path))

    if 'commit' in self._request.arguments:
      commit = self._request.arguments['commit']
    else:
      _log.warning('No commit given; refreshing from master. '
                   'This is probably NOT what you want.')
      commit = None

    server_instance = self._CreateServerInstance(commit)
    success = True
    try:
      if source_name == 'platform_bundle':
        data_source = server_instance.platform_bundle
      elif source_name == 'content_providers':
        data_source = server_instance.content_providers
      else:
        data_source = CreateDataSource(source_name, server_instance)

      class_name = data_source.__class__.__name__
      refresh_future = data_source.Refresh(source_path)
      assert isinstance(refresh_future, Future), (
          '%s.Refresh() did not return a Future' % class_name)
      timer = Timer()
      try:
        refresh_future.Get()
      except Exception as e:
        _log.error('%s: error %s' % (class_name, traceback.format_exc()))
        success = False
        if IsFileSystemThrottledError(e):
          return Response.ThrottledError('Throttled')
        raise
      finally:
        _log.info('Refreshing %s took %s' %
            (class_name, timer.Stop().FormatElapsed()))

    except:
      success = False
      # This should never actually happen.
      _log.error('uncaught error: %s' % traceback.format_exc())
      raise
    finally:
      _log.info('finished (%s)', 'success' if success else 'FAILED')
      return (Response.Ok('Success') if success else
              Response.InternalError('Failure'))

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
