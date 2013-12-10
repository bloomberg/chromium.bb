#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Run build_server so that files needed by tests are copied to the local
# third_party directory.
import build_server
build_server.main()

import json
import optparse
import os
import posixpath
import sys
import time
import unittest

from branch_utility import BranchUtility
from chroot_file_system import ChrootFileSystem
from extensions_paths import EXTENSIONS, PUBLIC_TEMPLATES
from fake_fetchers import ConfigureFakeFetchers
from handler import Handler
from link_error_detector import LinkErrorDetector, StringifyBrokenLinks
from local_file_system import LocalFileSystem
from local_renderer import LocalRenderer
from servlet import Request
from test_util import EnableLogging, DisableLogging, ChromiumPath

# Arguments set up if __main__ specifies them.
_EXPLICIT_TEST_FILES = None
_REBASE = False
_VERBOSE = False


def _ToPosixPath(os_path):
  return os_path.replace(os.sep, '/')

def _GetPublicFiles():
  '''Gets all public files mapped to their contents.
  '''
  public_path = ChromiumPath(PUBLIC_TEMPLATES)
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
      response = Handler(Request.ForTest('/_cron')).Get()
      self.assertEqual(200, response.status)
      self.assertEqual('Success', response.content.ToString())
    finally:
      print('Took %s seconds' % (time.time() - start_time))

    print("Checking for broken links...")
    start_time = time.time()
    link_error_detector = LinkErrorDetector(
        # TODO(kalman): Use of ChrootFileSystem here indicates a hack. Fix.
        ChrootFileSystem(LocalFileSystem.Create(), EXTENSIONS),
        lambda path: Handler(Request.ForTest(path)).Get(),
        'templates/public',
        ('extensions/index.html', 'apps/about_apps.html'))

    broken_links = link_error_detector.GetBrokenLinks()
    if broken_links and _VERBOSE:
      print('The broken links are:')
      print(StringifyBrokenLinks(broken_links))

    broken_links_set = set(broken_links)

    known_broken_links_path = os.path.join(
        sys.path[0], 'known_broken_links.json')
    try:
      with open(known_broken_links_path, 'r') as f:
        # The JSON file converts tuples and sets into lists, and for this
        # set union/difference logic they need to be converted back.
        known_broken_links = set(tuple(item) for item in json.load(f))
    except IOError:
      known_broken_links = set()

    newly_broken_links = broken_links_set - known_broken_links
    fixed_links = known_broken_links - broken_links_set

    if _REBASE:
      print('Rebasing broken links with %s newly broken and %s fixed links.' %
            (len(newly_broken_links), len(fixed_links)))
      with open(known_broken_links_path, 'w') as f:
        json.dump(broken_links, f,
                  indent=2, separators=(',', ': '), sort_keys=True)
    else:
      if fixed_links or newly_broken_links:
        print('Found %s broken links, and some have changed. '
              'If this is acceptable or expected then run %s with the --rebase '
              'option.' % (len(broken_links), os.path.split(__file__)[-1]))
      elif broken_links:
        print('Found %s broken links, but there were no changes.' %
              len(broken_links))
      if fixed_links:
        print('%s broken links have been fixed:' % len(fixed_links))
        print(StringifyBrokenLinks(fixed_links))
      if newly_broken_links:
        print('There are %s new broken links:' % len(newly_broken_links))
        print(StringifyBrokenLinks(newly_broken_links))
        self.fail('See logging for details.')

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
        if path.startswith(('apps/', 'extensions/')):
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
  parser.add_option('-a', '--all', action='store_true', default=False,
                    help='Render all pages, not just the one specified')
  parser.add_option('-r', '--rebase', action='store_true', default=False,
                    help='Rewrites the known_broken_links.json file with '
                         'the current set of broken links')
  parser.add_option('-v', '--verbose', action='store_true', default=False,
                    help='Show verbose output like currently broken links')
  (opts, args) = parser.parse_args()
  if not opts.all:
    _EXPLICIT_TEST_FILES = args
  _REBASE = opts.rebase
  _VERBOSE = opts.verbose
  # Kill sys.argv because we have our own flags.
  sys.argv = [sys.argv[0]]
  unittest.main()
