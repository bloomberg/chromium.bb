# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# These are fake fetchers that are used for testing and the preview server.
# They return canned responses for URLs. appengine_wrappers.py uses the fake
# fetchers if the App Engine imports fail.

import os
import re

import appengine_wrappers
from file_system import FileNotFoundError
import url_constants

class _FakeFetcher(object):
  def __init__(self, base_path):
    self._base_path = base_path

  def _ReadFile(self, path, mode='rb'):
    with open(os.path.join(self._base_path, path), mode) as f:
      return f.read()

  def _ListDir(self, path):
    return os.listdir(os.path.join(self._base_path, path))

  def _IsDir(self, path):
    return os.path.isdir(os.path.join(self._base_path, path))

  def _Stat(self, path):
    return int(os.stat(os.path.join(self._base_path, path)).st_mtime)

class FakeOmahaProxy(_FakeFetcher):
  def fetch(self, url):
    return self._ReadFile(os.path.join('server2',
                                       'test_data',
                                       'branch_utility',
                                       'first.json'))

class FakeSubversionServer(_FakeFetcher):
  def __init__(self, base_path):
    _FakeFetcher.__init__(self, base_path)
    self._base_pattern = re.compile(r'.*chrome/common/extensions/(.*)')

  def fetch(self, url):
    path = os.path.join(os.pardir, self._base_pattern.match(url).group(1))
    if self._IsDir(path):
      html = ['<html>Revision 000000']
      try:
        for f in self._ListDir(path):
          if f.startswith('.'):
            continue
          if self._IsDir(os.path.join(path, f)):
            html.append('<a>' + f + '/</a>')
          else:
            html.append('<a>' + f + '</a>')
        html.append('</html>')
        return '\n'.join(html)
      except OSError as e:
        raise FileNotFoundError('Listing %s failed: %s' (path, e))
    try:
      return self._ReadFile(path)
    except IOError as e:
      raise FileNotFoundError('Reading %s failed: %s' (path, e))

class FakeViewvcServer(_FakeFetcher):
  def __init__(self, base_path):
    _FakeFetcher.__init__(self, base_path)
    self._base_pattern = re.compile(r'.*chrome/common/extensions/(.*)')

  def fetch(self, url):
    path = os.path.join(os.pardir, self._base_pattern.match(url).group(1))
    if self._IsDir(path):
      html = ['<table><tbody><tr>...</tr>']
      for f in self._ListDir(path):
        if f.startswith('.'):
          continue
        html.append('<tr>')
        html.append('  <td><a>%s%s</a></td>' % (
            f, '/' if self._IsDir(os.path.join(path, f)) else ''))
        stat = self._Stat(os.path.join(path, f))
        html.append('  <td><a><strong>%s</strong></a></td>' % stat)
        html.append('<td></td><td></td><td></td>')
        html.append('</tr>')
      html.append('</tbody></table>')
      return '\n'.join(html)
    try:
      return self._ReadFile(path)
    except IOError as e:
      raise FileNotFoundError('Reading %s failed: %s' % (path, e))

class FakeGithubStat(_FakeFetcher):
  def fetch(self, url):
    return '{ "commit": { "tree": { "sha": 0} } }'

class FakeGithubZip(_FakeFetcher):
  def fetch(self, url):
    try:
      return self._ReadFile(os.path.join('server2',
                                         'test_data',
                                         'github_file_system',
                                         'apps_samples.zip'),
                            mode='rb')
    except IOError:
      return None

class FakeIssuesFetcher(_FakeFetcher):
  def fetch(self, url):
    return 'Status,Summary,ID'

def ConfigureFakeFetchers(docs):
  '''Configure the fake fetcher paths relative to the docs directory.
  '''
  appengine_wrappers.ConfigureFakeUrlFetch({
    url_constants.OMAHA_PROXY_URL: FakeOmahaProxy(docs),
    '%s/.*' % url_constants.SVN_URL: FakeSubversionServer(docs),
    '%s/.*' % url_constants.VIEWVC_URL: FakeViewvcServer(docs),
    '%s/commits/.*' % url_constants.GITHUB_URL: FakeGithubStat(docs),
    '%s/zipball' % url_constants.GITHUB_URL: FakeGithubZip(docs),
    re.escape(url_constants.OPEN_ISSUES_CSV_URL): FakeIssuesFetcher(docs),
    re.escape(url_constants.CLOSED_ISSUES_CSV_URL): FakeIssuesFetcher(docs)
  })
