#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import glob

import pyauto_functional  # Must be imported before pyauto
import pyauto
from pyauto_errors import JSONInterfaceError


class PDFTest(pyauto.PyUITest):
  """PDF related tests

  This test runs only on Google Chrome build, not on Chromium.
  """

  unloadable_pdfs = []

  def _PerformPDFAction(self, action, tab_index=0, windex=0):
    """Perform an action on a PDF tab.

    Args:
      action: one of "fitToHeight", "fitToWidth", "ZoomIn", "ZoomOut"
      tab_index: tab index  Defaults to 0
      windex: window index.  Defaults to 0
    """
    # Sometimes the zoom/fit bar is not fully loaded.  We need to wait for it to
    # load before we can perform actions.
    js = """if (document.getElementsByName("plugin") &&
      document.getElementsByName("plugin")[0])
      { window.domAutomationController.send("true"); }
      else {window.domAutomationController.send("false"); }"""
    try:
      self.assertTrue(self.WaitUntil(lambda: self.ExecuteJavascript(js,
        tab_index=tab_index, windex=windex), expect_retval="true"),
        msg='Could not find zoom/fit to page/width bar so we will not be able '
            'to peform the requested action')
    except JSONInterfaceError as e:
      # The PDF did not load, add it to the list and move on, we don't want the
      # test to abort so we can check all of the PDFs.
      PDFTest.unloadable_pdfs.append(self.GetActiveTabTitle())
      return
    assert action in ('fitToHeight', 'fitToWidth', 'ZoomIn', 'ZoomOut')
    js = 'document.getElementsByName("plugin")[0].%s()' % action
    # Add an empty string so that there's something to return back
    # (or else it hangs)
    return self.GetDOMValue('%s + ""' % js, tab_index)


  def testPDFRunner(self):
    """Navigate to pdf files and verify that browser doesn't crash"""
    # bail out if not a branded build
    properties = self.GetBrowserInfo()['properties']
    if properties['branding'] != 'Google Chrome':
      return
    breakpad_folder = properties['DIR_CRASH_DUMPS']
    old_dmp_files = glob.glob(os.path.join(breakpad_folder, '*.dmp'))
    pdf_files_path = os.path.join(self.DataDir(), 'pyauto_private', 'pdf')
    pdf_files = map(self.GetFileURLForPath,
                    glob.glob(os.path.join(pdf_files_path, '*.pdf')))
    # Add a pdf file over http:// to the list of pdf files.
    # crbug.com/70454
    pdf_files += ['http://www.irs.gov/pub/irs-pdf/fw4.pdf']

    # Some pdfs cause known crashes. Exclude them. crbug.com/63549
    exclude_list = ('nullip.pdf', 'sample.pdf')
    pdf_files = [x for x in pdf_files if
                 os.path.basename(x) not in exclude_list]

    PDFTest.unloadable_pdfs = []
    for url in pdf_files:
      self.AppendTab(pyauto.GURL(url))
    for tab_index in range(1, len(pdf_files) + 1):
      self.ActivateTab(tab_index)
      self._PerformPDFAction('fitToHeight', tab_index=tab_index)
      self._PerformPDFAction('fitToWidth', tab_index=tab_index)
    # Assert that there is at least 1 browser window.
    self.assertTrue(self.GetBrowserWindowCount(),
                    'Browser crashed, no window is open')
    # Verify there're no crash dump files
    for dmp_file in glob.glob(os.path.join(breakpad_folder, '*.dmp')):
      self.assertTrue(dmp_file in old_dmp_files,
          msg='Crash dump %s found' % dmp_file)
    self.assertEqual(len(PDFTest.unloadable_pdfs), 0, msg='The following PDFs '
                     'did not load: %s' % PDFTest.unloadable_pdfs)


if __name__ == '__main__':
  pyauto_functional.Main()
