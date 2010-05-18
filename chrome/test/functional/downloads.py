#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import filecmp
import logging
import os
import shutil
import sys
import tempfile
import time
import urllib

import pyauto_functional  # Must be imported before pyauto
import pyauto


class DownloadsTest(pyauto.PyUITest):
  """TestCase for Downloads."""

  def _EqualFileContents(self, file1, file2):
    """Determine if 2 given files have the same contents."""
    if not (os.path.exists(file1) and os.path.exists(file2)):
      return False
    return filecmp.cmp(file1, file2)

  def testNoDownloadWaitingNeeded(self):
    """Make sure "wait for downloads" returns quickly if we have none."""
    self.WaitForAllDownloadsToComplete()

  def testZip(self):
    """Download a zip and verify that it downloaded correctly.
       Also verify that the download shelf showed up.
    """
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_path = os.path.join(test_dir, 'a_zip_file.zip')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)

    self.DownloadAndWaitForStart(file_url)

    # Wait for the download to finish.
    self.WaitForAllDownloadsToComplete()

    # Verify that the download shelf is visible
    self.assertTrue(self.IsDownloadShelfVisible())

    # Verify that the file was correctly downloaded
    self.assertTrue(os.path.exists(downloaded_pkg))
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg))

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
      self.DownloadAndWaitForStart(file_url)

    self.WaitForAllDownloadsToComplete()

    # Verify that all files exist and have the right name
    for filename in renamed_files:
      self.assertTrue(os.path.exists(filename))
      os.path.exists(filename) and os.remove(filename)

  def testCrazyFilenames(self):
    """Test downloading with filenames containing special chars.

       The files are created on the fly and cleaned after use.
    """
    download_dir = self.GetDownloadDirectory().value()
    filename = os.path.join(self.DataDir(), 'downloads', 'crazy_filenames.txt')
    crazy_filenames = self.EvalDataFrom(filename)
    logging.info('Testing with %d crazy filenames' % len(crazy_filenames))

    def _CreateFile(name):
      """Create and fill the given file with some junk."""
      fp = open(name, 'w')  # name could be unicode
      print >>fp, 'This is a junk file named %s. ' % repr(name) * 100
      fp.close()

    # Temp dir for hosting crazy filenames.
    temp_dir = tempfile.mkdtemp(prefix='download')
    # Windows has a dual nature dealing with unicode filenames.
    # While the files are internally saved as unicode, there's a non-unicode
    # aware API that returns a locale-dependent coding on the true unicode
    # filenames.  This messes up things.
    # Filesystem-interfacing functions like os.listdir() need to
    # be given unicode strings to "do the right thing" on win.
    # Ref: http://boodebr.org/main/python/all-about-python-and-unicode
    try:
      for filename in crazy_filenames:  # filename is unicode.
        utf8_filename = filename.encode('utf-8')
        file_path = os.path.join(temp_dir, utf8_filename)
        _CreateFile(os.path.join(temp_dir, filename))  # unicode file.
        file_url = self.GetFileURLForPath(file_path)
        downloaded_file = os.path.join(download_dir, filename)
        os.path.exists(downloaded_file) and os.remove(downloaded_file)
        self.DownloadAndWaitForStart(file_url)
      self.WaitForAllDownloadsToComplete()

      # Verify downloads.
      downloads = self.GetDownloadsInfo().Downloads()
      self.assertEqual(len(downloads), len(crazy_filenames))

      for filename in crazy_filenames:
        downloaded_file = os.path.join(download_dir, filename)
        self.assertTrue(os.path.exists(downloaded_file))
        self.assertTrue(   # Verify file contents.
            self._EqualFileContents(downloaded_file,
                                    os.path.join(temp_dir, filename)))
        os.path.exists(downloaded_file) and os.remove(downloaded_file)
    finally:
      shutil.rmtree(unicode(temp_dir))  # unicode so that win treats nicely.


if __name__ == '__main__':
  pyauto_functional.Main()
