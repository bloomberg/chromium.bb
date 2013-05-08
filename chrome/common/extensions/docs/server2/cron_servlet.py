# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time
import traceback

from appengine_wrappers import DeadlineExceededError, IsDevServer, logservice
from branch_utility import BranchUtility
from caching_file_system import CachingFileSystem
from github_file_system import GithubFileSystem
from object_store_creator import ObjectStoreCreator
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Servlet, Request, Response
from subversion_file_system import SubversionFileSystem
import svn_constants
from third_party.json_schema_compiler.memoize import memoize

def _CreateServerInstanceForChannel(channel, delegate):
  object_store_creator = ObjectStoreCreator(channel, start_empty=True)
  branch = (delegate.CreateBranchUtility(object_store_creator)
      .GetBranchForChannel(channel))
  host_file_system = CachingFileSystem(
      delegate.CreateHostFileSystemForBranch(branch),
      object_store_creator)
  app_samples_file_system = delegate.CreateAppSamplesFileSystem(
      object_store_creator)
  return ServerInstance(channel,
                        object_store_creator,
                        host_file_system,
                        app_samples_file_system)

class _SingletonRenderServletDelegate(RenderServlet.Delegate):
  def __init__(self, server_instance):
    self._server_instance = server_instance

  def CreateServerInstanceForChannel(self, channel):
    return self._server_instance

class CronServlet(Servlet):
  '''Servlet which runs a cron job.
  '''
  def __init__(self, request, delegate_for_test=None):
    Servlet.__init__(self, request)
    self._delegate = delegate_for_test or CronServlet.Delegate()

  class Delegate(object):
    '''Allow runtime dependencies to be overridden for testing.
    '''
    def CreateBranchUtility(self, object_store_creator):
      return BranchUtility.Create(object_store_creator)

    def CreateHostFileSystemForBranch(self, branch):
      return SubversionFileSystem.Create(branch)

    def CreateAppSamplesFileSystem(self, object_store_creator):
      # TODO(kalman): CachingFileSystem wrapper for GithubFileSystem, but it's
      # not supported yet (see comment there).
      return (EmptyDirFileSystem() if IsDevServer() else
              GithubFileSystem.Create(object_store_creator))

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
    channel = self._request.path.strip('/')
    logging.info('cron/%s: starting' % channel)

    # This is returned every time RenderServlet wants to create a new
    # ServerInstance.
    server_instance = _CreateServerInstanceForChannel(channel, self._delegate)

    def get_via_render_servlet(path):
      return RenderServlet(
          Request(path, self._request.host, self._request.headers),
          _SingletonRenderServletDelegate(server_instance)).Get()

    def run_cron_for_dir(d, path_prefix=''):
      success = True
      start_time = time.time()
      files = [f for f in server_instance.content_cache.GetFromFileListing(d)
               if not f.endswith('/')]
      logging.info('cron/%s: rendering %s files from %s...' % (
          channel, len(files), d))
      try:
        for i, f in enumerate(files):
          error = None
          path = '%s%s' % (path_prefix, f)
          try:
            response = get_via_render_servlet(path)
            if response.status != 200:
              error = 'Got %s response' % response.status
          except DeadlineExceededError:
            logging.error(
                'cron/%s: deadline exceeded rendering %s (%s of %s): %s' % (
                    channel, path, i + 1, len(files), traceback.format_exc()))
            raise
          except error:
            pass
          if error:
            logging.error('cron/%s: error rendering %s: %s' % (
                channel, path, error))
            success = False
      finally:
        logging.info('cron/%s: rendering %s files from %s took %s seconds' % (
            channel, len(files), d, time.time() - start_time))
      return success

    success = True
    try:
      # Render all of the publicly accessible files.
      for path, path_prefix in (
          # Note: rendering the public templates will pull in all of the private
          # templates.
          (svn_constants.PUBLIC_TEMPLATE_PATH, ''),
          # Note: rendering the public templates will have pulled in the .js
          # and manifest.json files (for listing examples on the API reference
          # pages), but there are still images, CSS, etc.
          (svn_constants.STATIC_PATH, 'static/'),
          (svn_constants.EXAMPLES_PATH, 'extensions/examples/')):
          # Note: don't try to short circuit any of this stuff. We want to run
          # the cron for all the directories regardless of intermediate
          # failures.
        success = run_cron_for_dir(path, path_prefix=path_prefix) and success

      # TODO(kalman): Generic way for classes to request cron access. The next
      # two special cases are ugly. It would potentially greatly speed up cron
      # runs, too.

      # Extension examples have zip files too. Well, so do apps, but the app
      # file system doesn't get the Offline treatment so they don't need cron.
      manifest_json = '/manifest.json'
      example_zips = [
          '%s.zip' % filename[:-len(manifest_json)]
          for filename in server_instance.content_cache.GetFromFileListing(
              svn_constants.EXAMPLES_PATH)
          if filename.endswith(manifest_json)]
      logging.info('cron/%s: rendering %s example zips...' % (
          channel, len(example_zips)))
      start_time = time.time()
      try:
        success = success and all(
            get_via_render_servlet('extensions/examples/%s' % z).status == 200
            for z in example_zips)
      finally:
        logging.info('cron/%s: rendering %s example zips took %s seconds' % (
            channel, len(example_zips), time.time() - start_time))

      # Also trigger a redirect so that PathCanonicalizer has an opportunity to
      # cache file listings.
      logging.info('cron/%s: triggering a redirect...' % channel)
      redirect_response = get_via_render_servlet('storage.html')
      success = success and redirect_response.status == 302
    except DeadlineExceededError:
      success = False

    logging.info('cron/%s: finished' % channel)

    return (Response.Ok('Success') if success else
            Response.InternalError('Failure'))
