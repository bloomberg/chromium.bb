#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import shutil
import tempfile
import unittest
import vulcanize_gn


_HERE_DIR = os.path.dirname(__file__)


class VulcanizeGnTest(unittest.TestCase):
  def setUp(self):
    self._out_folder = None
    self._tmp_dirs = []
    self._tmp_src_dir = None

  def tearDown(self):
    for tmp_dir in self._tmp_dirs:
      shutil.rmtree(tmp_dir)

  def _write_file_to_src_dir(self, file_name, file_contents):
    if not self._tmp_src_dir:
      self._tmp_src_dir = self._create_tmp_dir()
    with open(os.path.join(self._tmp_src_dir, file_name), 'w') as tmp_file:
      tmp_file.write(file_contents)

  def _create_tmp_dir(self):
    # TODO(dbeam): support cross-drive paths (i.e. d:\ vs c:\).
    tmp_dir = tempfile.mkdtemp(dir=_HERE_DIR)
    self._tmp_dirs.append(tmp_dir)
    return tmp_dir

  def _read_out_file(self, file_name):
    assert self._out_folder
    return open(os.path.join(self._out_folder, file_name), 'r').read()

  def _run_vulcanize(self, depfile, html_in_file, html_out_file, js_out_file):
    # TODO(dbeam): make it possible to _run_vulcanize twice? Is that useful?
    assert not self._out_folder
    self._out_folder = self._create_tmp_dir()
    vulcanize_gn.main([
      '--depfile', os.path.join(self._out_folder,'depfile.d'),
      '--html_in_file', html_in_file,
      '--html_out_file', html_out_file,
      '--host', 'fake-host',
      '--input', self._tmp_src_dir,
      '--js_out_file', js_out_file,
      '--out_folder', self._out_folder,
    ])


  def testSimpleVulcanize(self):
    self._write_file_to_src_dir('element.html', '<div>got here!</div>')
    self._write_file_to_src_dir('element.js', "alert('yay');")
    self._write_file_to_src_dir('ui.html', '''
<link rel="import" href="element.html">
<script src="element.js"></script>
''')

    self._run_vulcanize(depfile='depfile.d',
                        html_in_file='ui.html',
                        html_out_file='fast.html',
                        js_out_file='fast.js')

    fast_html = self._read_out_file('fast.html')
    self.assertFalse('element.html' in fast_html)
    self.assertFalse('element.js' in fast_html)
    self.assertTrue('got here!' in fast_html)

    fast_js = self._read_out_file('fast.js')
    self.assertTrue('yay' in fast_js)

    depfile_d = self._read_out_file('depfile.d')
    self.assertTrue('element.html' in depfile_d)
    self.assertTrue('element.js' in depfile_d)


if __name__ == '__main__':
  unittest.main()
