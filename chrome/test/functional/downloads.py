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

  def testNoDownloadWaitingNeeded(self):
    """Make sure "wait for downloads" returns quickly if we have none."""
    self.WaitForAllDownloadsToComplete()

  def _DownloadAndWaitForStart(self, file_url):
    """Trigger download for the given url and wait for downloads to start.

    It waits for download by looking at the download info from Chrome, so
    anything which isn't registered by the history service won't be noticed.
    This is not thread-safe, but it's fine to call this method to start
    downloading multiple files in parallel. That is after starting a
    download, it's fine to start another one even if the first one hasn't
    completed.
    """
    num_downloads = len(self.GetDownloadsInfo().Downloads())
    self.NavigateToURL(file_url)  # Trigger download.
    # It might take a while for the download to kick in, hold on until then.
    self.assertTrue(self.WaitUntil(
        lambda: len(self.GetDownloadsInfo().Downloads()) == num_downloads + 1))

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

    self._DownloadAndWaitForStart(file_url)

    # Wait for the download to finish.
    self.WaitForAllDownloadsToComplete()

    # Verify that the download shelf is visible
    self.assertTrue(self.IsDownloadShelfVisible())

    # Verify that the file was correctly downloaded
    self.assertTrue(os.path.exists(downloaded_pkg))
    # print 'Download size is %d' % os.path.getsize(downloaded_pkg)
    self.assertEqual(golden_md5sum, self._ComputeMD5sum(downloaded_pkg))

  def testBigZip(self):
    # TODO: download something "pretty big".  The above test will
    # usually wortk even without the WaitForAllDownloadsToComplete().
    # Manual testing shows it isn't just a noop, but an explicit test
    # is needed here.  However, if the download is TOO big, we hit a
    # 30sec timeout in our automation proxy / IPC (didn't track down
    # where exactly).
    pass

  def testFileRenaming(self):
    """Test file renaming when downloading a already-existing filename."""
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_url = 'file://%s' % os.path.join(test_dir, 'a_zip_file.zip')
    download_dir = self.GetDownloadDirectory().value()

    num_times = 5
    assert num_times > 1, 'needs to be > 1 to work'
    renamed_files = []
    for i in range(num_times):
      expected_filename = os.path.join(download_dir, 'a_zip_file.zip')
      if i > 0:  # Files after first download are renamed.
        expected_filename = os.path.join(download_dir,
                                         'a_zip_file (%d).zip' % i)
        renamed_files.append(expected_filename)
      os.path.exists(expected_filename) and os.remove(expected_filename)
      self._DownloadAndWaitForStart(file_url)

    self.WaitForAllDownloadsToComplete()

    # Verify that all files exist and have the right name
    for filename in renamed_files:
      self.assertTrue(os.path.exists(filename))


if __name__ == '__main__':
  pyauto_functional.Main()
