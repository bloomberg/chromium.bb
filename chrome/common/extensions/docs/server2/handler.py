# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from StringIO import StringIO
import traceback

from appengine_wrappers import (
    DeadlineExceededError, IsDevServer, logservice, memcache, urlfetch, webapp)
from branch_utility import BranchUtility
from server_instance import ServerInstance
import svn_constants
import time

# The default channel to serve docs for if no channel is specified.
_DEFAULT_CHANNEL = 'stable'

class Handler(webapp.RequestHandler):
  # AppEngine instances should never need to call out to SVN. That should only
  # ever be done by the cronjobs, which then write the result into DataStore,
  # which is as far as instances look.
  #
  # Why? SVN is slow and a bit flaky. Cronjobs failing is annoying but
  # temporary. Instances failing affects users, and is really bad.
  #
  # Anyway - to enforce this, we actually don't give instances access to SVN.
  # If anything is missing from datastore, it'll be a 404. If the cronjobs
  # don't manage to catch everything - uhoh. On the other hand, we'll figure it
  # out pretty soon, and it also means that legitimate 404s are caught before a
  # round trip to SVN.
  #
  # However, we can't expect users of preview.py nor the dev server to run a
  # cronjob first, so, this is a hack allow that to be online all of the time.
  # TODO(kalman): achieve this via proper dependency injection.
  ALWAYS_ONLINE = IsDevServer()

  def __init__(self, request, response):
    super(Handler, self).__init__(request, response)

  def _HandleGet(self, path):
    channel_name, real_path = BranchUtility.SplitChannelNameFromPath(path)

    if channel_name == _DEFAULT_CHANNEL:
      self.redirect('/%s' % real_path)
      return

    if channel_name is None:
      channel_name = _DEFAULT_CHANNEL

    # TODO(kalman): Check if |path| is a directory and serve path/index.html
    # rather than special-casing apps/extensions.
    if real_path.strip('/') == 'apps':
      real_path = 'apps/index.html'
    if real_path.strip('/') == 'extensions':
      real_path = 'extensions/index.html'

    constructor = (
        ServerInstance.CreateOnline if Handler.ALWAYS_ONLINE else
        ServerInstance.GetOrCreateOffline)
    server_instance = constructor(channel_name)

    canonical_path = server_instance.path_canonicalizer.Canonicalize(real_path)
    if real_path != canonical_path:
      self.redirect(canonical_path)
      return

    server_instance.Get(real_path, self.request, self.response)

  def _HandleCron(self, path):
    # Cron strategy:
    #
    # Find all public template files and static files, and render them. Most of
    # the time these won't have changed since the last cron run, so it's a
    # little wasteful, but hopefully rendering is really fast (if it isn't we
    # have a problem).
    class MockResponse(object):
      def __init__(self):
        self.status = 200
        self.out = StringIO()
        self.headers = {}
      def set_status(self, status):
        self.status = status
      def clear(self, *args):
        pass

    class MockRequest(object):
      def __init__(self, path):
        self.headers = {}
        self.path = path
        self.url = '//localhost/%s' % path

    channel = path.split('/')[-1]
    logging.info('cron/%s: starting' % channel)

    server_instance = ServerInstance.CreateOnline(channel)

    def run_cron_for_dir(d, path_prefix=''):
      success = True
      start_time = time.time()
      files = [f for f in server_instance.content_cache.GetFromFileListing(d)
               if not f.endswith('/')]
      logging.info('cron/%s: rendering %s files from %s...' % (
          channel, len(files), d))
      for i, f in enumerate(files):
        error = None
        path = '%s%s' % (path_prefix, f)
        try:
          response = MockResponse()
          server_instance.Get(path, MockRequest(path), response)
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

    if success:
      self.response.status = 200
      self.response.out.write('Success')
    else:
      self.response.status = 500
      self.response.out.write('Failure')

    logging.info('cron/%s: finished' % channel)

  def _RedirectSpecialCases(self, path):
    google_dev_url = 'http://developer.google.com/chrome'
    if path == '/' or path == '/index.html':
      self.redirect(google_dev_url)
      return True

    if path == '/apps.html':
      self.redirect('/apps/about_apps.html')
      return True

    return False

  def _RedirectFromCodeDotGoogleDotCom(self, path):
    if (not self.request.url.startswith(('http://code.google.com',
                                         'https://code.google.com'))):
      return False

    new_url = 'http://developer.chrome.com/'

    # switch to https if necessary
    if (self.request.url.startswith('https')):
      new_url = new_url.replace('http', 'https', 1)

    path = path.split('/')
    if len(path) > 0 and path[0] == 'chrome':
      path.pop(0)
    for channel in BranchUtility.GetAllBranchNames():
      if channel in path:
        position = path.index(channel)
        path.pop(position)
        path.insert(0, channel)
    new_url += '/'.join(path)
    self.redirect(new_url)
    return True

  def get(self):
    path = self.request.path

    if path in ['favicon.ico', 'robots.txt']:
      response.set_status(404)
      return

    if self._RedirectSpecialCases(path):
      return

    if path.startswith('/cron'):
      # Crons often time out, and when they do *and* then eventually try to
      # flush logs they die. Turn off autoflush and manually do so at the end.
      logservice.AUTOFLUSH_ENABLED = False
      try:
        self._HandleCron(path)
      finally:
        logservice.flush()
      return

    # Redirect paths like "directory" to "directory/". This is so relative
    # file paths will know to treat this as a directory.
    if os.path.splitext(path)[1] == '' and path[-1] != '/':
      self.redirect(path + '/')
      return

    path = path.strip('/')
    if self._RedirectFromCodeDotGoogleDotCom(path):
      return

    self._HandleGet(path)
