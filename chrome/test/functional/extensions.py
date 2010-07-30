#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This module is a simple qa tool that installs extensions and tests whether the
browser crashes while visiting a list of urls.

Usage: python extensions.py -v

Note: This assumes that there is a directory of extensions called 'extensions'
and that there is a file of newline-separated urls to visit called 'urls.txt'
in the same directory as the script.
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

  def _ReturnCrashingExtensions(self, extensions, group_size, top_urls):
    """Install the given extensions in groups of group_size and return the
       group of extensions that crashes (if any).

    Args:
      extensions: A list of extensions to install.
      group_size: The number of extensions to install at one time.
      top_urls: The list of top urls to visit.

    Returns:
      The extensions in the crashing group or None if there is no crash.
    """
    curr_extension = 0
    num_extensions = len(extensions)
    self.RestartBrowser()

    while curr_extension < num_extensions:
      logging.debug('New group of %d extensions.' % group_size)
      group_end = curr_extension + group_size
      for extension in extensions[curr_extension:group_end]:
        logging.debug('Installing extension: %s' % extension)
        self.InstallExtension(pyauto.FilePath(extension), False)

      for url in top_urls:
        self.NavigateToURL(url)

      def _LogAndReturnCrashing():
        crashing_extensions = extensions[curr_extension:group_end]
        logging.debug('Crashing extensions: %s' % crashing_extensions)
        return crashing_extensions

      # If the browser has crashed, return the extensions in the failing group.
      try:
        num_browser_windows = self.GetBrowserWindowCount()
      except:
        return _LogAndReturnCrashing()
      else:
        if not num_browser_windows:
          return _LogAndReturnCrashing()

      curr_extension = group_end

    # None of the extensions crashed.
    return None

  def testExtensionCrashes(self):
    """Add top extensions; confirm browser stays up when visiting top urls"""
    self.assertTrue(os.path.exists(self.extensions_dir_),
             'The dir "%s" must exist' % os.path.abspath(self.extensions_dir_))
    self.assertTrue(os.path.exists(self.urls_file_),
             'The file "%s" must exist' % os.path.abspath(self.urls_file_))

    num_urls_to_visit = 100
    extensions_group_size = 20

    top_urls = [l.rstrip() for l in
                open(self.urls_file_).readlines()[:num_urls_to_visit]]

    failed_extensions = glob.glob(os.path.join(self.extensions_dir_, '*.crx'))
    group_size = extensions_group_size

    while(group_size and failed_extensions):
      failed_extensions = self._ReturnCrashingExtensions(
          failed_extensions, group_size, top_urls)
      group_size = group_size // 2

    self.assertFalse(failed_extensions,
                     'Extension(s) in failing group: %s' % failed_extensions)


if __name__ == '__main__':
  pyauto_functional.Main()
