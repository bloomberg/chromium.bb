# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import posixpath
import traceback

from app_yaml_helper import AppYamlHelper
from appengine_wrappers import IsDeadlineExceededError, logservice
from branch_utility import BranchUtility
from compiled_file_system import CompiledFileSystem
from data_source_registry import CreateDataSources
from environment import GetAppVersion, IsDevServer
from extensions_paths import EXAMPLES, PUBLIC_TEMPLATES, STATIC_DOCS
from file_system_util import CreateURLsFromPaths
from future import Future
from gcs_file_system_provider import CloudStorageFileSystemProvider
from github_file_system_provider import GithubFileSystemProvider
from host_file_system_provider import HostFileSystemProvider
from object_store_creator import ObjectStoreCreator
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Servlet, Request, Response
from special_paths import SITE_VERIFICATION_FILE
from timer import Timer, TimerClosure


class _SingletonRenderServletDelegate(RenderServlet.Delegate):
  def __init__(self, server_instance):
    self._server_instance = server_instance

  def CreateServerInstance(self):
    return self._server_instance

class _CronLogger(object):
  '''Wraps the logging.* methods to prefix them with 'cron' and flush
  immediately. The flushing is important because often these cron runs time
  out and we lose the logs.
  '''
  def info(self, msg, *args):    self._log(logging.info, msg, args)
  def warning(self, msg, *args): self._log(logging.warning, msg, args)
  def error(self, msg, *args):   self._log(logging.error, msg, args)

  def _log(self, logfn, msg, args):
    try:
      logfn('cron: %s' % msg, *args)
    finally:
      logservice.flush()

_cronlog = _CronLogger()

def _RequestEachItem(title, items, request_callback):
  '''Runs a task |request_callback| named |title| for each item in |items|.
  |request_callback| must take an item and return a servlet response.
  Returns true if every item was successfully run, false if any return a
  non-200 response or raise an exception.
  '''
  _cronlog.info('%s: starting', title)
  success_count, failure_count = 0, 0
  timer = Timer()
  try:
    for i, item in enumerate(items):
      def error_message(detail):
        return '%s: error rendering %s (%s of %s): %s' % (
            title, item, i + 1, len(items), detail)
      try:
        response = request_callback(item)
        if response.status == 200:
          success_count += 1
        else:
          _cronlog.error(error_message('response status %s' % response.status))
          failure_count += 1
      except Exception as e:
        _cronlog.error(error_message(traceback.format_exc()))
        failure_count += 1
        if IsDeadlineExceededError(e): raise
  finally:
    _cronlog.info('%s: rendered %s of %s with %s failures in %s',
        title, success_count, len(items), failure_count,
        timer.Stop().FormatElapsed())
  return success_count == len(items)

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
                                     max_trunk_revision=None):
      return HostFileSystemProvider(object_store_creator,
                                    max_trunk_revision=max_trunk_revision)

    def CreateGithubFileSystemProvider(self, object_store_creator):
      return GithubFileSystemProvider(object_store_creator)

    def CreateGCSFileSystemProvider(self, object_store_creator):
      return CloudStorageFileSystemProvider(object_store_creator)

    def GetAppVersion(self):
      return GetAppVersion()

  def Get(self):
    # Crons often time out, and if they do we need to make sure to flush the
    # logs before the process gets killed (Python gives us a couple of
    # seconds).
    #
    # So, manually flush logs at the end of the cron run. However, sometimes
    # even that isn't enough, which is why in this file we use _cronlog and
    # make it flush the log every time its used.
    logservice.AUTOFLUSH_ENABLED = False
    try:
      return self._GetImpl()
    except BaseException:
      _cronlog.error('Caught top-level exception! %s', traceback.format_exc())
    finally:
      logservice.flush()

  def _GetImpl(self):
    # Cron strategy:
    #
    # Find all public template files and static files, and render them. Most of
    # the time these won't have changed since the last cron run, so it's a
    # little wasteful, but hopefully rendering is really fast (if it isn't we
    # have a problem).
    _cronlog.info('starting')

    # This is returned every time RenderServlet wants to create a new
    # ServerInstance.
    #
    # TODO(kalman): IMPORTANT. This sometimes throws an exception, breaking
    # everything. Need retry logic at the fetcher level.
    server_instance = self._GetSafeServerInstance()
    trunk_fs = server_instance.host_file_system_provider.GetTrunk()

    def render(path):
      request = Request(path, self._request.host, self._request.headers)
      delegate = _SingletonRenderServletDelegate(server_instance)
      return RenderServlet(request, delegate).Get()

    def request_files_in_dir(path, prefix='', strip_ext=None):
      '''Requests every file found under |path| in this host file system, with
      a request prefix of |prefix|. |strip_ext| is an optional list of file
      extensions that should be stripped from paths before requesting.
      '''
      def maybe_strip_ext(name):
        if name == SITE_VERIFICATION_FILE or not strip_ext:
          return name
        base, ext = posixpath.splitext(name)
        return base if ext in strip_ext else name
      files = [maybe_strip_ext(name)
               for name, _ in CreateURLsFromPaths(trunk_fs, path, prefix)]
      return _RequestEachItem(path, files, render)

    results = []

    try:
      # Start running the hand-written Cron methods first; they can be run in
      # parallel. They are resolved at the end.
      def run_cron_for_future(target):
        title = target.__class__.__name__
        future, init_timer = TimerClosure(target.Cron)
        assert isinstance(future, Future), (
            '%s.Cron() did not return a Future' % title)
        def resolve():
          resolve_timer = Timer()
          try:
            future.Get()
          except Exception as e:
            _cronlog.error('%s: error %s' % (title, traceback.format_exc()))
            results.append(False)
            if IsDeadlineExceededError(e): raise
          finally:
            resolve_timer.Stop()
            _cronlog.info('%s took %s: %s to initialize and %s to resolve' %
                (title,
                 init_timer.With(resolve_timer).FormatElapsed(),
                 init_timer.FormatElapsed(),
                 resolve_timer.FormatElapsed()))
        return Future(callback=resolve)

      targets = (CreateDataSources(server_instance).values() +
                 [server_instance.content_providers,
                  server_instance.platform_bundle])
      title = 'initializing %s parallel Cron targets' % len(targets)
      _cronlog.info(title)
      timer = Timer()
      try:
        cron_futures = [run_cron_for_future(target) for target in targets]
      finally:
        _cronlog.info('%s took %s' % (title, timer.Stop().FormatElapsed()))

      # Samples are too expensive to run on the dev server, where there is no
      # parallel fetch.
      #
      # XXX(kalman): Currently samples are *always* too expensive to fetch, so
      # disabling them for now. It won't break anything so long as we're still
      # not enforcing that everything gets cached for normal instances.
      if False:  # should be "not IsDevServer()":
        # Fetch each individual sample file.
        results.append(request_files_in_dir(EXAMPLES,
                                            prefix='extensions/examples'))

      # Resolve the hand-written Cron method futures.
      title = 'resolving %s parallel Cron targets' % len(targets)
      _cronlog.info(title)
      timer = Timer()
      try:
        for future in cron_futures:
          future.Get()
      finally:
        _cronlog.info('%s took %s' % (title, timer.Stop().FormatElapsed()))

    except:
      results.append(False)
      # This should never actually happen (each cron step does its own
      # conservative error checking), so re-raise no matter what it is.
      _cronlog.error('uncaught error: %s' % traceback.format_exc())
      raise
    finally:
      success = all(results)
      _cronlog.info('finished (%s)', 'success' if success else 'FAILED')
      return (Response.Ok('Success') if success else
              Response.InternalError('Failure'))

  def _GetSafeServerInstance(self):
    '''Returns a ServerInstance with a host file system at a safe revision,
    meaning the last revision that the current running version of the server
    existed.
    '''
    delegate = self._delegate

    # IMPORTANT: Get a ServerInstance pinned to the most recent revision, not
    # HEAD. These cron jobs take a while and run very frequently such that
    # there is usually one running at any given time, and eventually a file
    # that we're dealing with will change underneath it, putting the server in
    # an undefined state.
    server_instance_near_head = self._CreateServerInstance(
        self._GetMostRecentRevision())

    app_yaml_handler = AppYamlHelper(
        server_instance_near_head.object_store_creator,
        server_instance_near_head.host_file_system_provider)

    if app_yaml_handler.IsUpToDate(delegate.GetAppVersion()):
      return server_instance_near_head

    # The version in app.yaml is greater than the currently running app's.
    # The safe version is the one before it changed.
    safe_revision = app_yaml_handler.GetFirstRevisionGreaterThan(
        delegate.GetAppVersion()) - 1

    _cronlog.info('app version %s is out of date, safe is %s',
        delegate.GetAppVersion(), safe_revision)

    return self._CreateServerInstance(safe_revision)

  def _GetMostRecentRevision(self):
    '''Gets the revision of the most recent patch submitted to the host file
    system. This is similar to HEAD but it's a concrete revision so won't
    change as the cron runs.
    '''
    head_fs = (
        self._CreateServerInstance(None).host_file_system_provider.GetTrunk())
    return head_fs.Stat('').version

  def _CreateServerInstance(self, revision):
    '''Creates a ServerInstance pinned to |revision|, or HEAD if None.
    NOTE: If passed None it's likely that during the cron run patches will be
    submitted at HEAD, which may change data underneath the cron run.
    '''
    object_store_creator = ObjectStoreCreator(start_empty=True)
    branch_utility = self._delegate.CreateBranchUtility(object_store_creator)
    host_file_system_provider = self._delegate.CreateHostFileSystemProvider(
        object_store_creator, max_trunk_revision=revision)
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
