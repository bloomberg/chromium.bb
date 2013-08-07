#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run build_server so that files needed by tests are copied to the local
# third_party directory.
import build_server
build_server.main()

import optparse
import os
import posixpath
import sys
import time
import unittest

from branch_utility import BranchUtility
from link_error_detector import LinkErrorDetector, StringifyBrokenLinks
from local_file_system import LocalFileSystem
from local_renderer import LocalRenderer
from fake_fetchers import ConfigureFakeFetchers
from handler import Handler
from servlet import Request
from test_util import EnableLogging, DisableLogging

# Arguments set up if __main__ specifies them.
_EXPLICIT_TEST_FILES = None

def _ToPosixPath(os_path):
  return os_path.replace(os.sep, '/')

def _GetPublicFiles():
  '''Gets all public files mapped to their contents.
  '''
  public_path = os.path.join(sys.path[0], os.pardir, 'templates', 'public')
  public_files = {}
  for path, dirs, files in os.walk(public_path, topdown=True):
    dirs[:] = [d for d in dirs if d != '.svn']
    relative_posix_path = _ToPosixPath(path[len(public_path):])
    for filename in files:
      with open(os.path.join(path, filename), 'r') as f:
        public_files['/'.join((relative_posix_path, filename))] = f.read()
  return public_files

class IntegrationTest(unittest.TestCase):
  def setUp(self):
    ConfigureFakeFetchers()

  @EnableLogging('info')
  def testCronAndPublicFiles(self):
    '''Runs cron then requests every public file. Cron needs to be run first
    because the public file requests are offline.
    '''
    if _EXPLICIT_TEST_FILES is not None:
      return

    print('Running cron...')
    start_time = time.time()
    try:
      response = Handler(Request.ForTest('/_cron/stable')).Get()
      self.assertEqual(200, response.status)
      self.assertEqual('Success', response.content.ToString())
    finally:
      print('Took %s seconds' % (time.time() - start_time))

    print("Checking for broken links...")
    start_time = time.time()
    link_error_detector = LinkErrorDetector(
        LocalFileSystem(os.path.join(sys.path[0], os.pardir, os.pardir)),
        lambda path: Handler(Request.ForTest(path)).Get(),
        'templates/public',
        ('extensions/index.html', 'apps/about_apps.html'))

    broken_links = link_error_detector.GetBrokenLinks()
    if broken_links:
      # TODO(jshumway): Test should fail when broken links are detected.
      print('Warning: Found %d broken links:' % (
        len(broken_links)))
      print(StringifyBrokenLinks(broken_links))

    print('Took %s seconds.' % (time.time() - start_time))

    print('Searching for orphaned pages...')
    start_time = time.time()
    orphaned_pages = link_error_detector.GetOrphanedPages()
    if orphaned_pages:
      # TODO(jshumway): Test should fail when orphaned pages are detected.
      print('Warning: Found %d orphaned pages:' % len(orphaned_pages))
      for page in orphaned_pages:
        print(page)
    print('Took %s seconds.' % (time.time() - start_time))

    public_files = _GetPublicFiles()

    print('Rendering %s public files...' % len(public_files.keys()))
    start_time = time.time()
    try:
      for path, content in public_files.iteritems():
        if path.endswith('redirects.json'):
          continue
        def check_result(response):
          self.assertEqual(200, response.status,
              'Got %s when rendering %s' % (response.status, path))
          # This is reaaaaally rough since usually these will be tiny templates
          # that render large files. At least it'll catch zero-length responses.
          self.assertTrue(len(response.content) >= len(content),
              'Content was "%s" when rendering %s' % (response.content, path))

        check_result(Handler(Request.ForTest(path)).Get())

        # Make sure that leaving out the .html will temporarily redirect to the
        # path with the .html.
        if path != '/404.html':
          redirect_result = Handler(
              Request.ForTest(posixpath.splitext(path)[0])).Get()
          self.assertEqual((path, False), redirect_result.GetRedirect())

        # Make sure including a channel will permanently redirect to the same
        # path without a channel.
        for channel in BranchUtility.GetAllChannelNames():
          redirect_result = Handler(
              Request.ForTest('%s/%s' % (channel, path))).Get()
          self.assertEqual((path, True), redirect_result.GetRedirect())

        # Samples are internationalized, test some locales.
        if path.endswith('/samples.html'):
          for lang in ['en-US', 'es', 'ar']:
            check_result(Handler(Request.ForTest(
                path,
                headers={'Accept-Language': '%s;q=0.8' % lang})).Get())
    finally:
      print('Took %s seconds' % (time.time() - start_time))

  # TODO(kalman): Move this test elsewhere, it's not an integration test.
  # Perhaps like "presubmit_tests" or something.
  def testExplicitFiles(self):
    '''Tests just the files in _EXPLICIT_TEST_FILES.
    '''
    if _EXPLICIT_TEST_FILES is None:
      return
    for filename in _EXPLICIT_TEST_FILES:
      print('Rendering %s...' % filename)
      start_time = time.time()
      try:
        response = LocalRenderer.Render(_ToPosixPath(filename))
        self.assertEqual(200, response.status)
        self.assertTrue(response.content != '')
      finally:
        print('Took %s seconds' % (time.time() - start_time))

    # TODO(jshumway): Check page for broken links (currently prohibited by the
    # time it takes to render the pages).

  @DisableLogging('warning')
  def testFileNotFound(self):
    response = Handler(Request.ForTest('/extensions/notfound.html')).Get()
    self.assertEqual(404, response.status)

if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option('-a', '--all', action='store_true', default=False)
  (opts, args) = parser.parse_args()
  if not opts.all:
    _EXPLICIT_TEST_FILES = args
  # Kill sys.argv because we have our own flags.
  sys.argv = [sys.argv[0]]
  unittest.main()
