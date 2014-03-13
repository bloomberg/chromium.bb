#!/usr/bin/env python
# Copyright 2013 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

import hashlib
import json
import logging
import os
import shutil
import subprocess
import sys
import tempfile
import unittest

BASE_DIR = os.path.dirname(os.path.abspath(__file__))
GOOGLETEST_DIR = os.path.dirname(BASE_DIR)
ROOT_DIR = os.path.dirname(GOOGLETEST_DIR)

sys.path.insert(0, ROOT_DIR)
sys.path.insert(0, os.path.join(ROOT_DIR, 'tests'))

import isolateserver
import run_isolated
import trace_test_util


class FixTestCases(unittest.TestCase):
  def setUp(self):
    self.tempdir = tempfile.mkdtemp(prefix='fix_test_case')
    self.srcdir = os.path.join(self.tempdir, 'srcdir')
    os.mkdir(self.srcdir)

  def tearDown(self):
    if self.tempdir:
      if VERBOSE:
        # If -v is used, this means the user wants to do further analysis on
        # the data.
        print('Leaking %s' % self.tempdir)
      else:
        shutil.rmtree(self.tempdir)

  def _run(self, cmd):
    if VERBOSE:
      cmd = cmd + ['--verbose'] * 3
    logging.info(cmd)
    proc = subprocess.Popen(
        [sys.executable] + cmd,
        cwd=self.srcdir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT)
    out = proc.communicate()[0]
    if VERBOSE:
      print '\n-----'
      print out.strip()
      print '-----\n'
    self.assertEqual(0, proc.returncode)
    return out

  @trace_test_util.check_can_trace
  def test_simple(self):
    # Create a directory with nothing in it and progressively add more stuff.
    isolate = os.path.join(self.srcdir, 'gtest_fake_pass.isolate')
    condition = 'OS=="linux" and chromeos==1'
    with open(isolate, 'w') as f:
      # Write a minimal .isolate file.
      f.write(str({
        'conditions': [
          [condition, {
            'variables': {
              'command': [
                'run_test_cases.py', 'gtest_fake_pass.py',
              ],
            },
          }],
        ],
      }))
    def _copy(filename):
      shutil.copy(
          os.path.join(BASE_DIR, 'gtest_fake', filename),
          os.path.join(self.srcdir, filename))
    _copy('gtest_fake_base.py')
    _copy('gtest_fake_pass.py')
    shutil.copy(
        os.path.join(GOOGLETEST_DIR, 'run_test_cases.py'),
        os.path.join(self.srcdir, 'run_test_cases.py'))
    # Deploy run_isolated with dependencies as zip into srcdir.
    run_isolated.get_as_zip_package(executable=False).zip_into_file(
        os.path.join(self.srcdir, 'run_isolated.zip'))

    logging.debug('1. Create a .isolated file out of the .isolate file.')
    isolated = os.path.join(self.srcdir, 'gtest_fake_pass.isolated')
    out = self._run(
        [
          os.path.join(ROOT_DIR, 'isolate.py'),
          'check', '-i', isolate, '-s', isolated,
          '--config-variable', 'OS', 'linux',
          '--config-variable', 'chromeos', '1',
        ])
    if not VERBOSE:
      self.assertEqual('', out)

    logging.debug('2. Run fix_test_cases.py on it.')
    # Give up on looking at stdout.
    cmd = [
      os.path.join(GOOGLETEST_DIR, 'fix_test_cases.py'),
      '-s', isolated,
      '--trace-blacklist', '.*\\.run_test_cases',
    ]
    _ = self._run(cmd)

    logging.debug('3. Asserting the content of the .isolated file.')
    with open(isolated) as f:
      actual_isolated = json.load(f)
    gtest_fake_base_py = os.path.join(self.srcdir, 'gtest_fake_base.py')
    gtest_fake_pass_py = os.path.join(self.srcdir, 'gtest_fake_pass.py')
    run_isolated_zip = os.path.join(self.srcdir, 'run_isolated.zip')
    run_test_cases_py = os.path.join(self.srcdir, 'run_test_cases.py')
    algo = hashlib.sha1
    expected_isolated = {
      u'algo': u'sha-1',
      u'command': [u'run_test_cases.py', u'gtest_fake_pass.py'],
      u'files': {
        u'gtest_fake_base.py': {
          u'm': 416,
          u'h': unicode(isolateserver.hash_file(gtest_fake_base_py, algo)),
          u's': os.stat(gtest_fake_base_py).st_size,
        },
        u'gtest_fake_pass.py': {
          u'm': 488,
          u'h': unicode(isolateserver.hash_file(gtest_fake_pass_py, algo)),
          u's': os.stat(gtest_fake_pass_py).st_size,
        },
        u'run_isolated.zip': {
          u'm': 416,
          u'h': unicode(isolateserver.hash_file(run_isolated_zip, algo)),
          u's': os.stat(run_isolated_zip).st_size,
        },
        u'run_test_cases.py': {
          u'm': 488,
          u'h': unicode(isolateserver.hash_file(run_test_cases_py, algo)),
          u's': os.stat(run_test_cases_py).st_size,
        },
      },
      u'relative_cwd': u'.',
      u'version': unicode(isolateserver.ISOLATED_FILE_VERSION),
    }
    if sys.platform == 'win32':
      for value in expected_isolated['files'].itervalues():
        self.assertTrue(value.pop('m'))
    self.assertEqual(expected_isolated, actual_isolated)

    # Now verify the .isolate file was updated! (That's the magical part where
    # you say wow!)
    with open(isolate) as f:
      actual = eval(f.read(), {'__builtins__': None}, None)
    expected = {
      'conditions': [
        [condition, {
          'variables': {
            'command': [
              'run_test_cases.py', 'gtest_fake_pass.py'
            ],
            'isolate_dependency_tracked': [
              'gtest_fake_base.py',
              'gtest_fake_pass.py',
              'run_isolated.zip',
              'run_test_cases.py',
            ],
          },
        }],
      ],
    }
    self.assertEqual(expected, actual)


if __name__ == '__main__':
  VERBOSE = '-v' in sys.argv
  if VERBOSE:
    unittest.TestCase.maxDiff = None
  logging.basicConfig(level=logging.DEBUG if VERBOSE else logging.ERROR)
  unittest.main()
