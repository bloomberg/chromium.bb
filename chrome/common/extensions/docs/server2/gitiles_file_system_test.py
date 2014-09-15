#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import base64
import json
import unittest

from extensions_paths import SERVER2
from file_system import StatInfo
from future import Future
from gitiles_file_system import (_CreateStatInfo,
                                 _ParseGitilesJson,
                                 GitilesFileSystem)
from path_util import IsDirectory
from test_file_system import TestFileSystem
from test_util import ReadFile


_BASE_URL = ''
_REAL_DATA_DIR = 'chrome/common/extensions/docs/templates/public/extensions/'
_TEST_DATA = (SERVER2, 'test_data', 'gitiles_file_system', 'public_extensions')
# GitilesFileSystem expects file content to be encoded in base64.
_TEST_FS = {
  'test1.txt': base64.b64encode('test1'),
  'dir1': {
    'test2.txt': base64.b64encode('test2'),
    'dir2': {
      'test3.txt': base64.b64encode('test3')
    }
  }
}


class _Response(object):
  def __init__(self, content=''):
    self.content = content
    self.status_code = 200


class DownloadError(Exception):
  pass


class _FakeGitilesFetcher(object):
  def __init__(self, fs):
    self._fs = fs

  def FetchAsync(self, url, access_token=None):
    def resolve():
      assert '?' in url
      if url == _BASE_URL + '?format=JSON':
        return _Response(json.dumps({'commit': 'a_commit'}))
      path, fmt = url.split('?')
      # Fetch urls are of the form <base_url>/<path>. We only want <path>.
      path = path.split('/', 1)[1]
      if path == _REAL_DATA_DIR:
        return _Response(ReadFile(*_TEST_DATA))
      # ALWAYS skip not found here.
      content = self._fs.Read((path,),
                              skip_not_found=True).Get().get(path, None)
      if content is None:
        # GitilesFS expects a DownloadError if the file wasn't found.
        raise DownloadError
      # GitilesFS expects directory content as a JSON string.
      if 'JSON' in fmt:
        content = json.dumps({
          'entries': [{
            # GitilesFS expects directory names to not have a trailing '/'.
            'name': name.rstrip('/'),
            'type': 'tree' if IsDirectory(name) else 'blob'
          } for name in content]
        })
      return _Response(content)
    return Future(callback=resolve)


class GitilesFileSystemTest(unittest.TestCase):
  def setUp(self):
    fetcher = _FakeGitilesFetcher(TestFileSystem(_TEST_FS))
    self._gitiles_fs = GitilesFileSystem(fetcher, _BASE_URL, 'master', None)

  def testParseGitilesJson(self):
    test_json = '\n'.join([
      ')]}\'',
      json.dumps({'commit': 'blah'})
    ])
    self.assertEqual(_ParseGitilesJson(test_json), {'commit': 'blah'})

  def testCreateStatInfo(self):
    test_json = '\n'.join([
      ')]}\'',
      json.dumps({
        'id': 'some_long_string',
        'entries': [
          {
            'mode': 33188,
            'type': 'blob',
              'id': 'long_id',
            'name': '.gitignore'
          },
          {
            'mode': 33188,
            'type': 'blob',
              'id': 'another_long_id',
            'name': 'PRESUBMIT.py'
          },
          {
            'mode': 33188,
            'type': 'blob',
              'id': 'yali',
            'name': 'README'
          }
        ]
      })
    ])
    expected_stat_info = StatInfo('some_long_string', {
      '.gitignore': 'long_id',
      'PRESUBMIT.py': 'another_long_id',
      'README': 'yali'
    })
    self.assertEqual(_CreateStatInfo(test_json), expected_stat_info)

  def testRead(self):
    # Read a top-level file.
    f = self._gitiles_fs.Read(['test1.txt'])
    self.assertEqual(f.Get(), {'test1.txt': 'test1'})
    # Read a top-level directory.
    f = self._gitiles_fs.Read(['dir1/'])
    self.assertEqual(f.Get(), {'dir1/': sorted(['test2.txt', 'dir2/'])})
    # Read a nested file.
    f = self._gitiles_fs.Read(['dir1/test2.txt'])
    self.assertEqual(f.Get(), {'dir1/test2.txt': 'test2'})
    # Read a nested directory.
    f = self._gitiles_fs.Read(['dir1/dir2/'])
    self.assertEqual(f.Get(), {'dir1/dir2/': ['test3.txt']})
    # Read multiple paths.
    f = self._gitiles_fs.Read(['test1.txt', 'dir1/test2.txt'])
    self.assertEqual(f.Get(), {'test1.txt': 'test1', 'dir1/test2.txt': 'test2'})
    # Test skip not found.
    f = self._gitiles_fs.Read(['fakefile'], skip_not_found=True)
    self.assertEqual(f.Get(), {})

  def testGetCommitID(self):
    self.assertEqual(self._gitiles_fs.GetCommitID().Get(), 'a_commit')

  def testStat(self):
    self.assertEqual(self._gitiles_fs.Stat(_REAL_DATA_DIR).version,
                     'ec21e736a3f00db2c0580e3cf71d91951656caec')

  def testGetIdentity(self):
    # Test that file systems at different commits still have the same identity.
    other_gitiles_fs = GitilesFileSystem.Create(commit='abcdefghijklmnop')
    self.assertEqual(self._gitiles_fs.GetIdentity(),
                     other_gitiles_fs.GetIdentity())

    yet_another_gitiles_fs = GitilesFileSystem.Create(branch='different')
    self.assertNotEqual(self._gitiles_fs.GetIdentity(),
                        yet_another_gitiles_fs.GetIdentity())

if __name__ == '__main__':
  unittest.main()
