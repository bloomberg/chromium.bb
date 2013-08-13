# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time
import traceback

from app_yaml_helper import AppYamlHelper
from appengine_wrappers import (
    GetAppVersion, IsDeadlineExceededError, IsDevServer, logservice)
from branch_utility import BranchUtility
from compiled_file_system import CompiledFileSystem
from empty_dir_file_system import EmptyDirFileSystem
from file_system_util import CreateURLsFromPaths
from github_file_system import GithubFileSystem
from host_file_system_creator import HostFileSystemCreator
from object_store_creator import ObjectStoreCreator
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Servlet, Request, Response
import svn_constants

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
  start_time = time.time()
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
          _cronlog.error(error_message('response status %s', response.status))
          failure_count += 1
      except Exception as e:
        _cronlog.error(error_message(traceback.format_exc()))
        failure_count += 1
        if IsDeadlineExceededError(e): raise
  finally:
    elapsed_seconds = time.time() - start_time
    _cronlog.info('%s: rendered %s of %s with %s failures in %s seconds',
        title, success_count, len(items), failure_count, elapsed_seconds);
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

    def CreateHostFileSystemCreator(self, object_store_creator):
      return HostFileSystemCreator(object_store_creator)

    def CreateAppSamplesFileSystem(self, object_store_creator):
      # TODO(kalman): CachingFileSystem wrapper for GithubFileSystem, but it's
      # not supported yet (see comment there).
      return (EmptyDirFileSystem() if IsDevServer() else
              GithubFileSystem.Create(object_store_creator))

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

    def render(path):
      request = Request(path, self._request.host, self._request.headers)
      delegate = _SingletonRenderServletDelegate(server_instance)
      return RenderServlet(request, delegate).Get()

    def request_files_in_dir(path, prefix=''):
      '''Requests every file found under |path| in this host file system, with
      a request prefix of |prefix|.
      '''
      files = [name for name, _ in
          CreateURLsFromPaths(server_instance.host_file_system, path, prefix)]
      return _RequestEachItem(path, files, render)

    results = []

    try:
      # Rendering the public templates will also pull in all of the private
      # templates.
      results.append(request_files_in_dir(svn_constants.PUBLIC_TEMPLATE_PATH))

      # Rendering the public templates will have pulled in the .js and
      # manifest.json files (for listing examples on the API reference pages),
      # but there are still images, CSS, etc.
      results.append(request_files_in_dir(svn_constants.STATIC_PATH,
                                          prefix='static/'))

      # Samples are too expensive to run on the dev server, where there is no
      # parallel fetch.
      if not IsDevServer():
        # Fetch each individual sample file.
        results.append(request_files_in_dir(svn_constants.EXAMPLES_PATH,
                                            prefix='extensions/examples/'))

        # Fetch the zip file of each example (contains all the individual
        # files).
        example_zips = []
        for root, _, files in server_instance.host_file_system.Walk(
            svn_constants.EXAMPLES_PATH):
          example_zips.extend(
              root + '.zip' for name in files if name == 'manifest.json')
        results.append(_RequestEachItem(
            'example zips',
            example_zips,
            lambda path: render('extensions/examples/' + path)))

      _cronlog.info('Redirector: starting')
      start_time = time.time()
      try:
        server_instance.redirector.Cron()
      except Exception as e:
        _cronlog.error('Redirector: error %s', traceback.format_exc())
        results.append(False)
        if IsDeadlineExceededError(e): raise
      finally:
        _cronlog.info('Redirector: took %s seconds', time.time() - start_time)

    except Exception:
      results.append(False)
      # This should never actually happen (each cron step does its own
      # conservative error checking), so re-raise no matter what it is.
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
    server_instance_at_head = self._CreateServerInstance(None)

    app_yaml_handler = AppYamlHelper(
        svn_constants.APP_YAML_PATH,
        server_instance_at_head.host_file_system,
        server_instance_at_head.object_store_creator,
        server_instance_at_head.host_file_system_creator)

    if app_yaml_handler.IsUpToDate(delegate.GetAppVersion()):
      # TODO(kalman): return a new ServerInstance at an explicit revision in
      # case the HEAD version changes underneath us.
      return server_instance_at_head

    # The version in app.yaml is greater than the currently running app's.
    # The safe version is the one before it changed.
    safe_revision = app_yaml_handler.GetFirstRevisionGreaterThan(
        delegate.GetAppVersion()) - 1

    _cronlog.info('app version %s is out of date, safe is %s',
        delegate.GetAppVersion(), safe_revision)

    return self._CreateServerInstance(safe_revision)

  def _CreateServerInstance(self, revision):
    object_store_creator = ObjectStoreCreator(start_empty=True)
    branch_utility = self._delegate.CreateBranchUtility(object_store_creator)
    host_file_system_creator = self._delegate.CreateHostFileSystemCreator(
        object_store_creator)
    host_file_system = host_file_system_creator.Create(revision=revision)
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
