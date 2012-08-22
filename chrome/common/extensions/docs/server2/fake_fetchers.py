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

def _ReadFile(path):
  with open(path, 'r') as f:
    return f.read()

class FakeOmahaProxy(object):
  def fetch(self, url):
    return _ReadFile(os.path.join('test_data', 'branch_utility', 'first.json'))

class FakeSubversionServer(object):
  def __init__(self):
    self._base_pattern = re.compile(r'.*chrome/common/extensions/(.*)')

  def fetch(self, url):
    path = os.path.join(
        os.pardir, os.pardir, self._base_pattern.match(url).group(1))
    if os.path.isdir(path):
      html = ['<html>Revision 000000']
      try:
        for f in os.listdir(path):
          if f.startswith('.'):
            continue
          if os.path.isdir(os.path.join(path, f)):
            html.append('<a>' + f + '/</a>')
          else:
            html.append('<a>' + f + '</a>')
        html.append('</html>')
        return '\n'.join(html)
      except OSError:
        raise FileNotFoundError(path)
    try:
      return _ReadFile(path)
    except IOError:
      raise FileNotFoundError(path)

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

class FakeGithubStat(object):
  def fetch(self, url):
    return '{ "commit": { "tree": { "sha": 0} } }'

class FakeGithubZip(object):
  def fetch(self, url):
    return _ReadFile(os.path.join('test_data',
                                  'github_file_system',
                                  'apps_samples.zip'))

def ConfigureFakeFetchers():
  appengine_wrappers.ConfigureFakeUrlFetch({
    url_constants.OMAHA_PROXY_URL: FakeOmahaProxy(),
    '%s/.*' % url_constants.SVN_URL: FakeSubversionServer(),
    '%s/.*' % url_constants.VIEWVC_URL: FakeViewvcServer(),
    '%s/commits/.*' % url_constants.GITHUB_URL: FakeGithubStat(),
    '%s/zipball' % url_constants.GITHUB_URL: FakeGithubZip()
  })
