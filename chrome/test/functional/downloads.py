#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import os
import sys
import time
import urllib

import pyauto_functional  # Must be imported before pyauto
import pyauto


class DownloadsTest(pyauto.PyUITest):
  """TestCase for Downloads."""

  def _ComputeMD5sum(self, filename):
    """Determine md5 checksum for the contents of |filename|."""
    md5 = hashlib.md5()
    md5.update(open(filename, 'rb').read())
    return md5.hexdigest()

  def testZip(self):
    """Download a zip and verify that it downloaded correctly.
       Also verify that the download shelf showed up.
    """
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    checksum_file = os.path.join(test_dir, 'a_zip_file.md5sum')
    file_url = 'file://%s' % os.path.join(test_dir, 'a_zip_file.zip')
    golden_md5sum = urllib.urlopen(checksum_file).read()
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')

    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)

    # Download
    self.NavigateToURL(file_url)

    # TODO: Remove this sleep. Wait until all downloads finish.
    time.sleep(4)

    # Verify that the download shelf is visible
    self.assertTrue(self.IsDownloadShelfVisible())

    # Verify that the file was correctly downloaded
    self.assertTrue(os.path.exists(downloaded_pkg))
    self.assertEqual(golden_md5sum, self._ComputeMD5sum(downloaded_pkg))


if __name__ == '__main__':
  pyauto_functional.Main()

