#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
import subprocess
import tempfile
import unittest
import urllib2

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PARENT_DIR = os.path.dirname(SCRIPT_DIR)

sys.path.append(PARENT_DIR)

import httpd


class HTTPDTest(unittest.TestCase):
  def setUp(self):
    self.server = httpd.LocalHTTPServer('.', 0)

  def tearDown(self):
    self.server.Shutdown()

  def testQuit(self):
    urllib2.urlopen(self.server.GetURL('?quit=1'))
    self.server.process.join(10)  # Wait 10 seconds for the process to finish.
    self.assertFalse(self.server.process.is_alive())


class RunTest(unittest.TestCase):
  def setUp(self):
    self.process = None
    self.tempscript = None

  def tearDown(self):
    if self.process and self.process.returncode is None:
      self.process.kill()
    if self.tempscript:
      os.remove(self.tempscript)

  def _Run(self, args=None):
    args = args or []
    run_py = os.path.join(PARENT_DIR, 'run.py')
    cmd = [sys.executable, run_py]
    cmd.extend(args)
    self.process = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                    stderr=subprocess.PIPE)
    stdout, stderr = self.process.communicate()
    try:
      self.assertEqual(0, self.process.returncode)
    except AssertionError:
      print 'subprocess failed: %s:\nstdout: %s\nstderr:%s\n' % (
          ' '.join(cmd), stdout, stderr)
    return stdout

  def _WriteTempScript(self, script):
    fd, filename = tempfile.mkstemp(suffix='.py')
    lines = script.splitlines()
    assert len(lines[0]) == 0  # First line is always empty.
    # Count the number of spaces after the first newline.
    m = re.match(r'\s*', lines[1])
    assert m
    indent = len(m.group(0))
    script = '\n'.join(line[indent:] for line in lines[1:])

    os.write(fd, script)
    os.close(fd)
    self.tempscript = filename

  def testQuit(self):
    self._WriteTempScript(r"""
      import sys
      import time
      import urllib2

      print 'running tempscript'
      sys.stdout.flush()
      f = urllib2.urlopen(sys.argv[-1] + '?quit=1')
      f.read()
      f.close()
      time.sleep(10)

      # Should be killed before this prints.
      print 'Not killed yet.'
      sys.stdout.flush()
      sys.exit(0)
    """)
    stdout = self._Run(['--', sys.executable, self.tempscript])
    self.assertTrue('running tempscript' in stdout)
    self.assertTrue("Not killed yet" not in stdout)

  def testSubprocessDies(self):
    self._WriteTempScript(r"""
      import sys
      print 'running tempscript'
      sys.stdout.flush()
      sys.exit(1)
    """)
    stdout = self._Run(['--', sys.executable, self.tempscript])
    self.assertTrue('running tempscript' in stdout)

if __name__ == '__main__':
  unittest.main()
