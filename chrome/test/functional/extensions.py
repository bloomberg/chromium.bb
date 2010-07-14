#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module is a simple qa tool that installs extensions and tests whether the
browser crashes while visiting a list of urls.

Usage: python extensions.py -v

Note: This assumes that there is a directory of extensions in called
'extensions' and that there is a file of newline-separated urls to visit called
'urls.txt' in the same directory as the script.
"""

import glob
import logging
import os
import sys

import pyauto_functional # must be imported before pyauto
import pyauto


class ExtensionsTest(pyauto.PyUITest):
  """Test of extensions."""
  # TODO: provide a way in pyauto to pass args to a test and take these as args
  extensions_dir_ = 'extensions'  # The directory of extensions
  urls_file_ = 'urls.txt'         # The file which holds a list of urls to visit

  def testExtensionCrashes(self):
    """Add top extensions; confirm browser stays up when visiting top urls"""
    self.assertTrue(os.path.exists(self.extensions_dir_),
             'The dir "%s" must exist' % os.path.abspath(self.extensions_dir_))
    self.assertTrue(os.path.exists(self.urls_file_),
             'The file "%s" must exist' % os.path.abspath(self.urls_file_))

    extensions_group_size = 20
    num_urls_to_visit = 100

    extensions = glob.glob(os.path.join(self.extensions_dir_, '*.crx'))
    top_urls = [l.rstrip() for l in open(self.urls_file_).readlines()]

    curr_extension = 0
    num_extensions = len(extensions)

    while curr_extension < num_extensions:
      logging.debug('New group of %d extensions.' % extensions_group_size)
      group_end = curr_extension + extensions_group_size
      for extension in extensions[curr_extension:group_end]:
        logging.debug('Installing extension: %s' % extension)
        self.InstallExtension(pyauto.FilePath(extension), False)

      # Navigate to the top urls and verify there is still one window
      for url in top_urls[:num_urls_to_visit]:
        self.NavigateToURL(url)
      # Assert that there is at least 1 browser window.
      self.assertTrue(self.GetBrowserWindowCount(),
                      'Extensions in failing group: %s' %
                      extensions[curr_extension:group_end])
      curr_extension = group_end


if __name__ == '__main__':
  pyauto_functional.Main()
