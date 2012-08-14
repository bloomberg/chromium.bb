#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
from StringIO import StringIO
import unittest

import appengine_memcache as memcache
import appengine_wrappers
import url_constants

KNOWN_FAILURES = [
  # Apps samples fails because it requires fetching data from github.com.
  # This should be tested though: http://crbug.com/141910.
  'apps/samples.html',
]

def _ReadFile(path):
  with open(path, 'r') as f:
    return f.read()

class FakeOmahaProxy(object):
  def fetch(self, url):
    return _ReadFile(os.path.join('test_data', 'branch_utility', 'first.json'))

class FakeViewvcServer(object):
  def __init__(self):
    self._base_pattern = re.compile(r'.*chrome/common/extensions/(.*)')

  def fetch(self, url):
    path = os.path.join(
        os.pardir, os.pardir, self._base_pattern.match(url).group(1))
    if os.path.isdir(path):
      html = ['<html><td>Directory revision:</td><td><a>000000</a></td>']
      for f in os.listdir(path):
        if f.startswith('.'):
          continue
        html.append('<td><a name="%s"></a></td>' % f)
        if os.path.isdir(os.path.join(path, f)):
          html.append('<td><a title="dir"><strong>000000</strong></a></td>')
        else:
          html.append('<td><a title="file"><strong>000000</strong></a></td>')
      html.append('</html>')
      return '\n'.join(html)
    return _ReadFile(path)

class FakeSubversionServer(object):
  def __init__(self):
    self._base_pattern = re.compile(r'.*chrome/common/extensions/(.*)')

  def fetch(self, url):
    path = os.path.join(
        os.pardir, os.pardir, self._base_pattern.match(url).group(1))
    if os.path.isdir(path):
      html = ['<html>Revision 000000']
      for f in os.listdir(path):
        if f.startswith('.'):
          continue
        if os.path.isdir(os.path.join(path, f)):
          html.append('<a>' + f + '/</a>')
        else:
          html.append('<a>' + f + '</a>')
      html.append('</html>')
      return '\n'.join(html)
    return _ReadFile(path)

class FakeGithub(object):
  def fetch(self, url):
    return '{ "commit": { "tree": { "sha": 0} } }'

appengine_wrappers.ConfigureFakeUrlFetch({
  url_constants.OMAHA_PROXY_URL: FakeOmahaProxy(),
  '%s/.*' % url_constants.SVN_URL: FakeSubversionServer(),
  '%s/.*' % url_constants.VIEWVC_URL: FakeViewvcServer(),
  '%s/.*' % url_constants.GITHUB_URL: FakeGithub()
})

# Import Handler later because it immediately makes a request to github. We need
# the fake urlfetch to be in place first.
from handler import Handler

class _MockResponse(object):
  def __init__(self):
    self.status = 200
    self.out = StringIO()

  def set_status(self, status):
    self.status = status

class _MockRequest(object):
  def __init__(self, path):
    self.headers = {}
    self.path = path

class IntegrationTest(unittest.TestCase):
  def testAll(self):
    logging.getLogger().setLevel(logging.ERROR)
    base_path = os.path.join('templates', 'public')
    for path, dirs, files in os.walk(base_path):
      for name in files:
        filename = os.path.join(path, name)
        if (filename.split('/', 2)[-1] in KNOWN_FAILURES or
            '.' in path or
            name.startswith('.')):
          continue
        request = _MockRequest(filename.split('/', 2)[-1])
        response = _MockResponse()
        Handler(request, response, local_path='../..').get()
        self.assertEqual(200, response.status)
        self.assertTrue(response.out.getvalue())

  def test404(self):
    request = _MockRequest('junk.html')
    bad_response = _MockResponse()
    Handler(request, bad_response, local_path='../..').get()
    self.assertEqual(404, bad_response.status)
    self.assertTrue(bad_response.out.getvalue())

  def testLocales(self):
    # Use US English, Spanish, and Arabic.
    for lang in ['en-US', 'es', 'ar']:
      request = _MockRequest('extensions/samples.html')
      request.headers['Accept-Language'] = lang + ';q=0.8'
      response = _MockResponse()
      Handler(request, response, local_path='../..').get()
      self.assertEqual(200, response.status)
      self.assertTrue(response.out.getvalue())

  def testWarmupRequest(self):
    request = _MockRequest('_ah/warmup')
    response = _MockResponse()
    Handler(request, response, local_path='../..').get()
    self.assertEqual(200, response.status)
    # Test that the pages were rendered by checking the size of the output.
    # In python 2.6 there is no 'assertGreater' method.
    value = response.out.getvalue()
    self.assertTrue(len(value) > 100000,
                    msg='Response is too small, probably empty: %s' % value)

if __name__ == '__main__':
  unittest.main()
