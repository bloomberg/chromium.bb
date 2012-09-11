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

  def _ReadFile(self, path, mode='r'):
    with open(os.path.join(self._base_path, path), mode) as f:
      return f.read()

  def _ListDir(self, path):
    return os.listdir(os.path.join(self._base_path, path))

  def _IsDir(self, path):
    return os.path.isdir(os.path.join(self._base_path, path))

  def _Stat(self, path):
    return os.stat(os.path.join(self._base_path, path)).st_mtime

class FakeOmahaProxy(_FakeFetcher):
  def fetch(self, url):
    return self._ReadFile(os.path.join('test_data',
                                       'branch_utility',
                                       'first.json'))

class FakeSubversionServer(_FakeFetcher):
  def __init__(self, base_path):
    _FakeFetcher.__init__(self, base_path)
    self._base_pattern = re.compile(r'.*chrome/common/extensions/(.*)')

  def fetch(self, url):
    path = os.path.join(os.pardir,
                        os.pardir,
                        self._base_pattern.match(url).group(1))
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
      except OSError:
        raise FileNotFoundError(path)
    try:
      return self._ReadFile(path)
    except IOError:
      raise FileNotFoundError(path)

class FakeViewvcServer(_FakeFetcher):
  def __init__(self, base_path):
    _FakeFetcher.__init__(self, base_path)
    self._base_pattern = re.compile(r'.*chrome/common/extensions/(.*)')

  def fetch(self, url):
    path = os.path.join(os.pardir,
                        os.pardir,
                        self._base_pattern.match(url).group(1))
    if self._IsDir(path):
      html = ['<html><td>Directory revision:</td><td><a>%s</a></td>' %
              self._Stat(path)]
      for f in self._ListDir(path):
        if f.startswith('.'):
          continue
        html.append('<td><a name="%s"></a></td>' % f)
        stat = self._Stat(os.path.join(path, f))
        html.append('<td><a title="%s"><strong>%s</strong></a></td>' %
            ('dir' if self._IsDir(os.path.join(path, f)) else 'file', stat))
      html.append('</html>')
      return '\n'.join(html)
    try:
      return self._ReadFile(path)
    except IOError:
      raise FileNotFoundError(path)

class FakeGithubStat(_FakeFetcher):
  def fetch(self, url):
    return '{ "commit": { "tree": { "sha": 0} } }'

class FakeGithubZip(_FakeFetcher):
  def fetch(self, url):
    try:
      return self._ReadFile(os.path.join('test_data',
                                         'github_file_system',
                                         'apps_samples.zip'),
                            mode='rb')
    except IOError:
      return None

def ConfigureFakeFetchers(base_path):
  appengine_wrappers.ConfigureFakeUrlFetch({
    url_constants.OMAHA_PROXY_URL: FakeOmahaProxy(base_path),
    '%s/.*' % url_constants.SVN_URL: FakeSubversionServer(base_path),
    '%s/.*' % url_constants.VIEWVC_URL: FakeViewvcServer(base_path),
    '%s/commits/.*' % url_constants.GITHUB_URL: FakeGithubStat(base_path),
    '%s/zipball' % url_constants.GITHUB_URL: FakeGithubZip(base_path)
  })
