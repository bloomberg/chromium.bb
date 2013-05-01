# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time
import traceback

from appengine_wrappers import DeadlineExceededError, logservice
from branch_utility import BranchUtility
from render_servlet import RenderServlet
from server_instance import ServerInstance
from servlet import Servlet, Request, Response
import svn_constants

class CronServlet(Servlet):
  '''Servlet which runs a cron job.
  '''
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

    server_instance = ServerInstance.CreateOnline(channel)

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
            response = RenderServlet(Request(path, self._request.headers)).Get(
                server_instance=server_instance)
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
    for path, path_prefix in (
        # Note: rendering the public templates will pull in all of the private
        # templates.
        (svn_constants.PUBLIC_TEMPLATE_PATH, ''),
        # Note: rendering the public templates will have pulled in the .js and
        # manifest.json files (for listing examples on the API reference pages),
        # but there are still images, CSS, etc.
        (svn_constants.STATIC_PATH, 'static/'),
        (svn_constants.EXAMPLES_PATH, 'extensions/examples/')):
      try:
        # Note: don't try to short circuit any of this stuff. We want to run
        # the cron for all the directories regardless of intermediate failures.
        success = run_cron_for_dir(path, path_prefix=path_prefix) and success
      except DeadlineExceededError:
        success = False
        break

    logging.info('cron/%s: finished' % channel)

    return (Response.Ok('Success') if success else
            Response.InternalError('Failure'))
