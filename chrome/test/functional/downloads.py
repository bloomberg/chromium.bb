#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
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
      os.path.exists(filename) and os.remove(filename)

  def _EvalDataFrom(self, filename):
    """Return eval of python code from given file.

    The datastructure used in the file will be preserved.
    """
    data_file = os.path.join(self.DataDir(), 'downloads', filename)
    contents = open(data_file).read()
    try:
      ret = eval(contents, {'__builtins__': None}, None)
    except:
      print >>sys.stderr, '%s is an invalid data file.' % data_file
      raise
    return ret

  def testCrazyFilenames(self):
    """Test downloading with filenames containing special chars.

       The files are created on the fly and cleaned after use.
    """
    download_dir = self.GetDownloadDirectory().value()
    crazy_filenames = self._EvalDataFrom('crazy_filenames.txt')
    logging.info('Testing with %d crazy filenames' % len(crazy_filenames))

    def _CreateFile(name):
      """Create and fill the given file with some junk."""
      fp = open(name, 'w')
      print >>fp, 'This is a junk file named %s. ' % name * 100
      fp.close()

    # Temp dir for hosting crazy filenames.
    temp_dir = tempfile.mkdtemp(prefix='download')
    try:
      for filename in crazy_filenames:
        file_path = os.path.join(temp_dir, filename)
        _CreateFile(file_path)
        file_url = self.GetFileURLForPath(file_path)
        downloaded_file = os.path.join(download_dir, filename)
        os.path.exists(downloaded_file) and os.remove(downloaded_file)
        self._DownloadAndWaitForStart(file_url)
      self.WaitForAllDownloadsToComplete()

      # Verify downloads.
      downloads = self.GetDownloadsInfo().Downloads()
      self.assertEqual(len(downloads), len(crazy_filenames))

      for filename in crazy_filenames:
        downloaded_file = os.path.join(download_dir, filename)
        self.assertTrue(os.path.exists(downloaded_file))
        self.assertEqual(   # Verify checksum.
            self._ComputeMD5sum(downloaded_file),
            self._ComputeMD5sum(os.path.join(temp_dir, filename)))
        os.path.exists(downloaded_file) and os.remove(downloaded_file)
    finally:
      shutil.rmtree(temp_dir)


if __name__ == '__main__':
  pyauto_functional.Main()
