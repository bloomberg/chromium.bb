#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import optparse
import os
import sys
from StringIO import StringIO
import re
import unittest

# Run build_server so that files needed by tests are copied to the local
# third_party directory.
import build_server
build_server.main()

BASE_PATH = None
EXPLICIT_TEST_FILES = None

from fake_fetchers import ConfigureFakeFetchers
ConfigureFakeFetchers(sys.path[0])

# Import Handler later because it immediately makes a request to github. We need
# the fake urlfetch to be in place first.
from handler import Handler

class _MockResponse(object):
  def __init__(self):
    self.status = 200
    self.out = StringIO()
    self.headers = {}

  def set_status(self, status):
    self.status = status

class _MockRequest(object):
  def __init__(self, path):
    self.headers = {}
    self.path = path
    self.url = 'http://localhost' + path

class IntegrationTest(unittest.TestCase):
  def _TestSamplesLocales(self, sample_path, failures):
    # Use US English, Spanish, and Arabic.
    for lang in ['en-US', 'es', 'ar']:
      request = _MockRequest(sample_path)
      request.headers['Accept-Language'] = lang + ';q=0.8'
      response = _MockResponse()
      try:
        Handler(request, response, local_path=BASE_PATH).get()
        if 200 != response.status:
          failures.append(
              'Samples page with language %s does not have 200 status.'
              ' Status was %d.' %  (lang, response.status))
        if not response.out.getvalue():
          failures.append(
              'Rendering samples page with language %s produced no output.' %
                  lang)
      except Exception as e:
        failures.append('Error rendering samples page with language %s: %s' %
            (lang, e))

  def _RunPublicTemplatesTest(self):
    base_path = os.path.join(BASE_PATH,
                             'docs',
                             'server2',
                             'templates',
                             'public')
    if EXPLICIT_TEST_FILES is None:
      test_files = []
      for path, dirs, files in os.walk(base_path):
        for dir_ in dirs:
          if dir_.startswith('.'):
            dirs.remove(dir_)
        for name in files:
          if name.startswith('.') or name == '404.html':
            continue
          test_files.append(os.path.join(path, name)[len(base_path + os.sep):])
    else:
      test_files = EXPLICIT_TEST_FILES
    test_files = [f.replace(os.sep, '/') for f in test_files]
    failures = []
    for filename in test_files:
      request = _MockRequest(filename)
      response = _MockResponse()
      try:
        Handler(request, response, local_path=BASE_PATH).get()
        if 200 != response.status:
          failures.append('%s does not have 200 status. Status was %d.' %
              (filename, response.status))
        if not response.out.getvalue():
          failures.append('Rendering %s produced no output.' % filename)
        if filename.endswith('samples.html'):
          self._TestSamplesLocales(filename, failures)
      except Exception as e:
        failures.append('Error rendering %s: %s' % (filename, e))
    if failures:
      self.fail('\n'.join(failures))

  def testAllPublicTemplates(self):
    logging.getLogger().setLevel(logging.ERROR)
    logging_error = logging.error
    try:
      logging.error = self.fail
      self._RunPublicTemplatesTest()
    finally:
      logging.error = logging_error

  def testNonexistentFile(self):
    logging.getLogger().setLevel(logging.CRITICAL)
    request = _MockRequest('extensions/junk.html')
    bad_response = _MockResponse()
    Handler(request, bad_response, local_path=BASE_PATH).get()
    self.assertEqual(404, bad_response.status)
    request_404 = _MockRequest('404.html')
    response_404 = _MockResponse()
    Handler(request_404, response_404, local_path=BASE_PATH).get()
    self.assertEqual(200, response_404.status)
    self.assertEqual(response_404.out.getvalue(), bad_response.out.getvalue())

  def testCron(self):
    if EXPLICIT_TEST_FILES is not None:
      return
    logging_error = logging.error
    try:
      logging.error = self.fail
      request = _MockRequest('/cron/trunk')
      response = _MockResponse()
      Handler(request, response, local_path=BASE_PATH).get()
      self.assertEqual(200, response.status)
      self.assertEqual('Success', response.out.getvalue())
    finally:
      logging.error = logging_error

if __name__ == '__main__':
  parser = optparse.OptionParser()
  parser.add_option('-p',
                    '--path',
                    default=os.path.join(os.pardir, os.pardir))
  parser.add_option('-a',
                    '--all',
                    action='store_true',
                    default=False)
  (opts, args) = parser.parse_args()

  if not opts.all:
    EXPLICIT_TEST_FILES = args
  BASE_PATH = opts.path
  suite = unittest.TestSuite(tests=[
    IntegrationTest('testNonexistentFile'),
    IntegrationTest('testCron'),
    IntegrationTest('testAllPublicTemplates')
  ])
  result = unittest.TestResult()
  suite.run(result)
  if result.failures:
    print('*----------------------------------*')
    print('| integration_test.py has failures |')
    print('*----------------------------------*')
    for test, failure in result.failures:
      print(test)
      print(failure)
    exit(1)
  print('All integration tests passed!')
  exit(0)
