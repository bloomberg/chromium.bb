# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time
import traceback

from app_yaml_helper import AppYamlHelper
from appengine_wrappers import (
    GetAppVersion, DeadlineExceededError, IsDevServer, logservice)
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
    # Crons often time out, and when they do *and* then eventually try to
    # flush logs they die. Turn off autoflush and manually do so at the end.
    logservice.AUTOFLUSH_ENABLED = False
    try:
      return self._GetImpl()
    finally:
      logservice.flush()

  def _GetImpl(self):
    # Cron strategy:
    #
    # Find all public template files and static files, and render them. Most of
    # the time these won't have changed since the last cron run, so it's a
    # little wasteful, but hopefully rendering is really fast (if it isn't we
    # have a problem).
    logging.info('cron: starting')

    # This is returned every time RenderServlet wants to create a new
    # ServerInstance.
    server_instance = self._GetSafeServerInstance()

    def get_via_render_servlet(path):
      request = Request(path, self._request.host, self._request.headers)
      delegate = _SingletonRenderServletDelegate(server_instance)
      return RenderServlet(request, delegate).Get()

    def run_cron_for_dir(d, path_prefix=''):
      success = True
      start_time = time.time()
      files = dict(
          CreateURLsFromPaths(server_instance.host_file_system, d, path_prefix))
      logging.info('cron: rendering %s files from %s...' % (len(files), d))
      try:
        for i, path in enumerate(files):
          error = None
          try:
            response = get_via_render_servlet(path)
            if response.status != 200:
              error = 'Got %s response' % response.status
          except DeadlineExceededError:
            logging.error(
                'cron: deadline exceeded rendering %s (%s of %s): %s' % (
                    path, i + 1, len(files), traceback.format_exc()))
            raise
          except error:
            pass
          if error:
            logging.error('cron: error rendering %s: %s' % (path, error))
            success = False
      finally:
        logging.info('cron: rendering %s files from %s took %s seconds' % (
            len(files), d, time.time() - start_time))
      return success

    success = True
    try:
      # Render all of the publicly accessible files.
      cron_runs = [
        # Note: rendering the public templates will pull in all of the private
        # templates.
        (svn_constants.PUBLIC_TEMPLATE_PATH, ''),
        # Note: rendering the public templates will have pulled in the .js
        # and manifest.json files (for listing examples on the API reference
        # pages), but there are still images, CSS, etc.
        (svn_constants.STATIC_PATH, 'static/'),
      ]
      if not IsDevServer():
        cron_runs.append(
            (svn_constants.EXAMPLES_PATH, 'extensions/examples/'))

      # Note: don't try to short circuit any of this stuff. We want to run
      # the cron for all the directories regardless of intermediate
      # failures.
      for path, path_prefix in cron_runs:
        success = run_cron_for_dir(path, path_prefix=path_prefix) and success

      # TODO(kalman): Generic way for classes to request cron access. The next
      # two special cases are ugly. It would potentially greatly speed up cron
      # runs, too.

      # Extension examples have zip files too. Well, so do apps, but the app
      # file system doesn't get the Offline treatment so they don't need cron.
      if not IsDevServer():
        manifest_json = 'manifest.json'
        example_zips = []
        for root, _, files in server_instance.host_file_system.Walk(
            svn_constants.EXAMPLES_PATH):
          example_zips.extend(
              root + '.zip' for name in files if name == manifest_json)
        logging.info('cron: rendering %s example zips...' % len(example_zips))
        start_time = time.time()
        try:
          success = success and all(
              get_via_render_servlet('extensions/examples/%s' % z).status == 200
              for z in example_zips)
        finally:
          logging.info('cron: rendering %s example zips took %s seconds' % (
              len(example_zips), time.time() - start_time))

    except DeadlineExceededError:
      success = False

    logging.info('cron: running Redirector cron...')
    server_instance.redirector.Cron()

    logging.info('cron: finished (%s)' % ('success' if success else 'failure',))

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

    logging.info('cron: app version %s is out of date, safe is %s' % (
        delegate.GetAppVersion(), safe_revision))

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
