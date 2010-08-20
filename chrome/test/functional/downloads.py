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
    return filecmp.cmp(file1, file2, shallow=False)

  def _GetDownloadId(self, download_index=0):
    """Return the download id for the download at the given index.

    Args:
      download_index: The index of the download in the list of downloads.
                      Default is 0.
    """
    return self.GetDownloadsInfo().Downloads()[download_index]['id']

  def _GetAllDownloadIDs(self):
    """Return a list of all download ids."""
    return [download['id'] for download in self.GetDownloadsInfo().Downloads()]

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

  def testDownloadDangerousFiles(self):
    """Verify that we can download and save dangerous files."""
    test_dir = os.path.abspath('.')
    # This file is a .py file which is "dangerous"
    file_path = os.path.join(test_dir, 'downloads.py')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'downloads.py')
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)

    self.DownloadAndWaitForStart(file_url)
    self.PerformActionOnDownload(self._GetDownloadId(),
                                 'save_dangerous_download')
    self.WaitForDownloadToComplete(downloaded_pkg)

    # Verify that the file was downloaded.
    self.assertTrue(os.path.exists(downloaded_pkg))
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg))
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)

  def testRemoveDownload(self):
    """Verify that we can remove a download."""
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_path = os.path.join(test_dir, 'a_zip_file.zip')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)

    self.DownloadAndWaitForStart(file_url)
    self.PerformActionOnDownload(self._GetDownloadId(), 'remove')

    # The download is removed from downloads, but not from the disk.
    self.assertFalse(self.GetDownloadsInfo().Downloads())
    self.assertTrue(os.path.exists(downloaded_pkg))

  def testBigZip(self):
    """Verify that we can download a 1GB file.

    This test needs 2 GB of free space, 1 GB for the original zip file and
    another for the downloaded one.

    Note: This test increases automation timeout to 4 min.  Things might seem
          to hang.
    """
    size = 2**30  # 1 GB
    # Create a 1 GB file on the fly
    fd, file_path = tempfile.mkstemp(suffix='.zip', prefix='bigfile-')
    os.lseek(fd, size, 0)
    os.write(fd, 'a')
    os.close(fd)
    try:
      file_url = self.GetFileURLForPath(file_path)
      downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                    os.path.basename(file_path))
      os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
      self.DownloadAndWaitForStart(file_url)
      # Waiting for big file to download might exceed automation timeout.
      # Temporarily increase the automation timeout.
      new_timeout = 4 * 60 * 1000  # 4 min
      timeout_changer = pyauto.PyUITest.CmdExecutionTimeoutChanger(
          self, new_timeout)
      logging.info('Automation execution timeout has been increased. Things '
                   'might seem to be hung even though it might not really be.')
      self.WaitForAllDownloadsToComplete()
      del timeout_changer  # reset automation timeout
      # Verify that the file was correctly downloaded
      self.assertTrue(os.path.exists(downloaded_pkg),
                      'Downloaded file %s missing.' % downloaded_pkg)
      self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg),
                      'Downloaded file %s does not match original' %
                        downloaded_pkg)
    finally:  # Cleanup. Remove all big files.
      os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
      os.path.exists(file_path) and os.remove(file_path)


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

  def testDownloadsPersistence(self):
    """Verify that download history persists on session restart."""
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_url = self.GetFileURLForPath(os.path.join(test_dir, 'a_zip_file.zip'))
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertEqual(1, len(downloads))
    self.assertEqual('a_zip_file.zip', downloads[0]['file_name'])
    file_url = downloads[0]['url']
    self.RestartBrowser(clear_profile=False)
    # Trigger the download service to get loaded after restart.
    self.NavigateToURL('chrome://downloads/')
    # Verify that there's no download shelf anymore.
    self.assertFalse(self.IsDownloadShelfVisible(),
                     'Download shelf persisted browser restart.')
    # Verify that the download history persists.
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertEqual(1, len(downloads))
    self.assertEqual('a_zip_file.zip', downloads[0]['file_name'])
    self.assertEqual(file_url, downloads[0]['url'])


if __name__ == '__main__':
  pyauto_functional.Main()
