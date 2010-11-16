#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import glob

import pyauto_functional  # Must be imported before pyauto
import pyauto


class PDFTest(pyauto.PyUITest):
  """PDF related tests

  This test runs only on Google Chrome build, not on Chromium.
  """

  def testPDFRunner(self):
    """Navigate to pdf files and verify that browser doesn't crash"""
    # bail out if not a branded build
    properties = self.GetBrowserInfo()['properties']
    if properties['branding'] != 'Google Chrome':
      return
    pdf_files_path = os.path.join(self.DataDir(), 'pyauto_private', 'pdf')
    pdf_files = glob.glob(os.path.join(pdf_files_path, '*.pdf'))
    for pdf_file in pdf_files:
      url = self.GetFileURLForPath(os.path.join(pdf_files_path, pdf_file))
      self.AppendTab(pyauto.GURL(url))
    # Assert that there is at least 1 browser window.
    self.assertTrue(self.GetBrowserWindowCount(),
                    'Browser crashed, no window is open')


if __name__ == '__main__':
  pyauto_functional.Main()
