#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

# Run build_server so that files needed by tests are copied to the local
# third_party directory.
import build_server
build_server.main()

import logging
import optparse
import os
import sys
import time
import unittest

from cron_servlet import CronServlet
from local_renderer import LocalRenderer
from render_servlet import AlwaysOnline
from test_util import DisableLogging

# Arguments set up if __main__ specifies them.
_BASE_PATH = os.path.join(
    os.path.abspath(os.path.dirname(__file__)), os.pardir, os.pardir)
_EXPLICIT_TEST_FILES = None

def _GetPublicFiles():
  '''Gets all public files mapped to their contents.
  '''
  public_path = os.path.join(_BASE_PATH, 'docs', 'templates', 'public', '')
  public_files = {}
  for path, dirs, files in os.walk(public_path, topdown=True):
    dirs[:] = [d for d in dirs if d != '.svn']
    relative_path = path[len(public_path):]
    for filename in files:
      with open(os.path.join(path, filename), 'r') as f:
        public_files[os.path.join(relative_path, filename)] = f.read()
  return public_files

class IntegrationTest(unittest.TestCase):
  def setUp(self):
    self._renderer = LocalRenderer(_BASE_PATH)

  def testCronAndPublicFiles(self):
    '''Runs cron then requests every public file. Cron needs to be run first
    because the public file requests are offline.
    '''
    if _EXPLICIT_TEST_FILES is not None:
      return

    print('Running cron...')
    start_time = time.time()
    try:
      logging_info = logging.info
      logging.info = print
      response = self._renderer.Render('/stable', servlet=CronServlet)
      self.assertEqual(200, response.status)
      self.assertEqual('Success', response.content.ToString())
    finally:
      logging.info = logging_info
      print('Took %s seconds' % (time.time() - start_time))

    public_files = _GetPublicFiles()

    print('Rendering %s public files...' % len(public_files.keys()))
    start_time = time.time()
    try:
      for path, content in _GetPublicFiles().iteritems():
        def check_result(response):
          self.assertEqual(200, response.status,
              'Got %s when rendering %s' % (response.status, path))
          # This is reaaaaally rough since usually these will be tiny templates
          # that render large files. At least it'll catch zero-length responses.
          self.assertTrue(len(response.content) >= len(content),
              'Content was "%s" when rendering %s' % (response.content, path))
        check_result(self._renderer.Render(path))
        # Samples are internationalized, test some locales.
        if path.endswith('/samples.html'):
          for lang in ['en-US', 'es', 'ar']:
            check_result(self._renderer.Render(
                path, headers={'Accept-Language': '%s;q=0.8' % lang}))
    finally:
      print('Took %s seconds' % (time.time() - start_time))

  @AlwaysOnline
  def testExplicitFiles(self):
    '''Tests just the files in _EXPLICIT_TEST_FILES.
    '''
    if _EXPLICIT_TEST_FILES is None:
      return
    for filename in _EXPLICIT_TEST_FILES:
      print('Rendering %s...' % filename)
      start_time = time.time()
      try:
        response = self._renderer.Render(filename)
        self.assertEqual(200, response.status)
        self.assertTrue(response.content != '')
      finally:
        print('Took %s seconds' % (time.time() - start_time))

  @DisableLogging('warning')
  @AlwaysOnline
  def testFileNotFound(self):
    response = self._renderer.Render('/extensions/notfound.html')
    self.assertEqual(404, response.status)

if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option('-p', '--path', default=None)
  parser.add_option('-a', '--all', action='store_true', default=False)
  (opts, args) = parser.parse_args()
  if not opts.all:
    _EXPLICIT_TEST_FILES = args
  if opts.path is not None:
    _BASE_PATH = opts.path
  # Kill sys.argv because we have our own flags.
  sys.argv = [sys.argv[0]]
  unittest.main()
