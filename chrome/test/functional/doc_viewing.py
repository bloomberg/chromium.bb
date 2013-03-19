#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional  # must be imported before pyauto

import chromeos.file_browser
import pyauto
import test_utils


class DocViewingTest(pyauto.PyUITest):
  """Basic tests for ChromeOS document viewing.

  Requires ChromeOS to be logged in.
  """

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    extension_path = '/opt/google/chrome/extensions'\
        '/gbkeegbaiigmenfmjfclcdgdpimamgkj.crx'
    # If crx file with doesn't exist, component extensions should be used.
    if os.path.exists(extension_path):
      ext_id = self.InstallExtension(extension_path, from_webstore=True)
      self.assertTrue(ext_id, msg='Failed to install extension %s' %
                      extension_path)

  def _GetFullPageFileBrowser(self):
    """Display the full page file browser.

    Returns:
      ChromeosFileBrowser object.
    """
    self.NavigateToURL('chrome://files/#/Downloads')
    executor = pyauto.PyUITest.JavascriptExecutorInTab(self)
    file_browser = chromeos.file_browser.FileBrowser(self, executor)
    if file_browser.WaitUntilInitialized():
      return file_browser
    else:
      return None

  def testOpenOfficeFiles(self):
    """Test we can open office files from the file manager."""
    path = os.path.abspath(os.path.join(self.DataDir(),
                                        'pyauto_private', 'office'))
    # Copy sample files to Downloads directory.
    for (path, dirs, private_office_files) in os.walk(path):
      # Open sample files: .ppt, .pptx, .doc, .docx, xls, xlsx.
      for fname in private_office_files:
        test_utils.CopyFileFromDataDirToDownloadDir(self, os.path.join(path,
                                                                       fname))
        file_browser = self._GetFullPageFileBrowser()
        self.assertTrue(file_browser, msg='File browser failed to initialize.')

        def _SelectFile():
          try:
            file_browser.Select(fname)
            return True
          except AssertionError:
            return False

        self.assertTrue(self.WaitUntil(_SelectFile),
                        msg='"%s" does not exist.' % fname)
        file_browser.ExecuteDefaultTask()
        self.assertTrue(self.WaitUntil(self.GetActiveTabTitle,
                        expect_retval=fname),
                        msg='"%s" does not open.' % fname)
        # Close the document viewing tab after use.
        self.CloseTab(tab_index=1)


if __name__ == '__main__':
  pyauto_functional.Main()
