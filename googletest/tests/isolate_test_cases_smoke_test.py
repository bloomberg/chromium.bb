#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import hashlib
import json
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

GOOGLETEST_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT_DIR = os.path.dirname(GOOGLETEST_DIR)

sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(GOOGLETEST_DIR, '..', 'tests'))

import isolate
import trace_test_util
from googletest import trace_test_cases
from utils import file_path


class IsolateTestCases(unittest.TestCase):
  def setUp(self):
    self.tempdir = None

    self.initial_cwd = ROOT_DIR
    if sys.platform == 'win32':
      # Windows has no kernel mode concept of current working directory.
      self.initial_cwd = None

    # There's 2 kinds of references to python, self.executable,
    # self.real_executable. It depends how python was started and on which OS.
    self.executable = unicode(sys.executable)
    if sys.platform == 'darwin':
      # /usr/bin/python is a thunk executable that decides which version of
      # python gets executed.
      suffix = '.'.join(map(str, sys.version_info[0:2]))
      if os.access(self.executable + suffix, os.X_OK):
        # So it'll look like /usr/bin/python2.7
        self.executable += suffix

    self.real_executable = file_path.get_native_path_case(
        self.executable)
    # Make sure there's no environment variable that could cause side effects.
    os.environ.pop('GTEST_SHARD_INDEX', '')
    os.environ.pop('GTEST_TOTAL_SHARDS', '')

  def tearDown(self):
    if self.tempdir:
      if VERBOSE:
        # If -v is used, this means the user wants to do further analisys on
        # the data.
        print('Leaking %s' % self.tempdir)
      else:
        shutil.rmtree(self.tempdir)

  def _copy(self, root, *relpath):
    relpath = os.path.join(*relpath)
    shutil.copy(
        os.path.join(root, relpath),
        os.path.join(self.tempdir, relpath))

  @trace_test_util.check_can_trace
  def test_simple(self):
    # Create a directory and re-use tests/gtest_fake/gtest_fake_pass.isolate.
    # Warning: we need to copy the files around, since the original .isolate
    # file is modified.
    gtest_fake_base_py = os.path.join(
        'tests', 'gtest_fake', 'gtest_fake_base.py')
    gtest_fake_pass_py = os.path.join(
        'tests', 'gtest_fake', 'gtest_fake_pass.py')
    gtest_fake_pass_isolate = os.path.join(
        'tests', 'isolate_test_cases', 'gtest_fake_pass.isolate')

    self.tempdir = tempfile.mkdtemp(prefix='isolate_test_cases_test')
    os.mkdir(os.path.join(self.tempdir, 'isolated'))
    os.mkdir(os.path.join(self.tempdir, 'tests'))
    os.mkdir(os.path.join(self.tempdir, 'tests', 'gtest_fake'))
    os.mkdir(os.path.join(self.tempdir, 'tests', 'isolate_test_cases'))
    self._copy(ROOT_DIR, 'isolate.py')
    self._copy(GOOGLETEST_DIR, gtest_fake_base_py)
    self._copy(GOOGLETEST_DIR, gtest_fake_pass_isolate)
    self._copy(GOOGLETEST_DIR, gtest_fake_pass_py)

    basename = os.path.join(self.tempdir, 'isolated', 'gtest_fake_pass')
    isolated = basename + '.isolated'

    # Create a proper .isolated file.
    cmd = [
      sys.executable, 'isolate.py',
      'check',
      '--config-variable', 'OS', 'amiga',
      '--extra-variable', 'FLAG', 'run',
      '--isolate', os.path.join(self.tempdir, gtest_fake_pass_isolate),
      '--isolated', isolated,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    subprocess.check_call(cmd, cwd=ROOT_DIR)

    # Assert the content of the .isolated file.
    with open(isolated) as f:
      actual_isolated = json.load(f)
    root_dir_gtest_fake_pass_py = os.path.join(
        GOOGLETEST_DIR, gtest_fake_pass_py)
    rel_gtest_fake_pass_py = os.path.join(u'gtest_fake', 'gtest_fake_pass.py')
    expected_isolated = {
      u'algo': u'sha-1',
      u'command': [u'../gtest_fake/gtest_fake_pass.py'],
      u'files': {
        rel_gtest_fake_pass_py: {
          u'm': 488,
          u'h': unicode(hashlib.sha1(
              open(root_dir_gtest_fake_pass_py, 'rb').read()).hexdigest()),
          u's': os.stat(root_dir_gtest_fake_pass_py).st_size,
        },
      },
      u'relative_cwd': u'isolate_test_cases',
      u'version': unicode(isolate.isolateserver.ISOLATED_FILE_VERSION),
    }
    if sys.platform == 'win32':
      expected_isolated['files'][rel_gtest_fake_pass_py].pop('m')
    self.assertEqual(expected_isolated, actual_isolated)

    cmd = [
        sys.executable,
        os.path.join(GOOGLETEST_DIR, 'isolate_test_cases.py'),
        # Forces 4 parallel jobs.
        '--jobs', '4',
        '--isolated', isolated,
    ]
    if VERBOSE:
      cmd.extend(['-v'] * 3)
    logging.debug(' '.join(cmd))
    proc = subprocess.Popen(
        cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    out, err = proc.communicate() or ('', '')  # pylint is confused.
    logging.info(err)
    self.assertEqual(0, proc.returncode, (out, err))
    lines = out.splitlines()
    expected_out_re = [
      r'\[0/3\]   \d\.\d\ds ',
      r'\[1/3\]   \d+\.\d\ds .+',
      r'\[2/3\]   \d+\.\d\ds .+',
      r'\[3/3\]   \d+\.\d\ds .+',
    ]
    self.assertEqual(len(expected_out_re), len(lines), (out, err))
    for index in range(len(expected_out_re)):
      self.assertTrue(
          re.match('^%s$' % expected_out_re[index], lines[index]),
          '%d\n%r\n%r\n%r' % (
            index, expected_out_re[index], lines[index], out))
    # Junk is printed on win32.
    if sys.platform != 'win32' and not VERBOSE:
      self.assertEqual('', err)

    test_cases = (
      'Foo.Bar1',
      'Foo.Bar2',
      'Foo.Bar/3',
    )
    expected = {
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            'isolate_dependency_untracked': [
              '../gtest_fake/',
            ],
          },
        }],
      ],
    }
    for test_case in test_cases:
      tracename = trace_test_cases.sanitize_test_case_name(test_case)
      with open(basename + '.' + tracename + '.isolate', 'r') as f:
        result = eval(f.read(), {'__builtins__': None}, None)
        self.assertEqual(expected, result)

    # Now verify the .isolate file was updated! (That's the magical part where
    # you say wow!)
    with open(os.path.join(self.tempdir, gtest_fake_pass_isolate)) as f:
      actual = eval(f.read(), {'__builtins__': None}, None)
    expected = {
      'conditions': [
        ['OS=="amiga"', {
          'variables': {
            'isolate_dependency_untracked': [
              '../gtest_fake/',
            ],
          },
        }],
      ],
      'variables': {
        'command': ['../gtest_fake/gtest_fake_pass.py'],
        'isolate_dependency_tracked': [
          '../gtest_fake/gtest_fake_pass.py',
        ],
      },
    }
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  if VERBOSE:
    unittest.TestCase.maxDiff = None
  unittest.main()
