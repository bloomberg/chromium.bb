#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import commands
import filecmp
import logging
import os
import shutil
import sys
import tempfile
import urllib

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils
import test_utils


class DownloadsTest(pyauto.PyUITest):
  """TestCase for Downloads."""

  def setUp(self):
    pyauto.PyUITest.setUp(self)
    # Record all entries in the download dir
    download_dir = self.GetDownloadDirectory().value()
    self._existing_downloads = []
    if os.path.isdir(download_dir):
      self._existing_downloads += os.listdir(download_dir)
    self._files_to_remove = []  # Files to remove after browser shutdown

  def tearDown(self):
    # Cleanup all files we created in the download dir
    download_dir = self.GetDownloadDirectory().value()
    if os.path.isdir(download_dir):
      for name in os.listdir(download_dir):
        if name not in self._existing_downloads:
          self._files_to_remove.append(os.path.join(download_dir, name))
    pyauto.PyUITest.tearDown(self)
    # Delete all paths marked for deletion after browser shutdown.
    for item in self._files_to_remove:
      pyauto_utils.RemovePath(item)

  def _DeleteAfterShutdown(self, path):
    """Delete |path| after browser has been shut down.

    This is so that all handles to the path would have been gone by then.
    Silently Ignores errors, when the path does not exist, or if the path
    could not be deleted.
    """
    self._files_to_remove.append(path)

  def _ClearLocalDownloadState(self, path):
    """Prepare for downloading the given path.

    Clears the given path and the corresponding .crdownload, to prepare it to
    be downloaded.
    """
    os.path.exists(path) and os.remove(path)
    crdownload = path + '.crdownload'
    os.path.exists(crdownload) and os.remove(crdownload)

  def _GetDangerousDownload(self):
    """Returns the file path for a dangerous download for this OS."""
    sub_path = os.path.join(self.DataDir(), 'downloads', 'dangerous')
    if self.IsMac():
      return os.path.join(sub_path, 'invalid-dummy.dmg')
    return os.path.join(sub_path, 'dangerous.exe')

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

  def _MakeFile(self, size):
    """Make a file on-the-fly with the given size. Returns the path to the
       file.
    """
    fd, file_path = tempfile.mkstemp(suffix='.zip', prefix='file-downloads-')
    os.lseek(fd, size, 0)
    os.write(fd, 'a')
    os.close(fd)
    logging.debug('Created temporary file %s of size %d' % (file_path, size))
    return file_path

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
    self._ClearLocalDownloadState(downloaded_pkg)

    self.DownloadAndWaitForStart(file_url)

    # Wait for the download to finish.
    self.WaitForAllDownloadsToComplete()

    # Verify that the download shelf is visible
    self.assertTrue(self.IsDownloadShelfVisible())

    # Verify that the file was correctly downloaded
    self.assertTrue(os.path.exists(downloaded_pkg))
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg))

  def testZipInIncognito(self):
    """Download and verify a zip in incognito window."""
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_path = os.path.join(test_dir, 'a_zip_file.zip')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    self._ClearLocalDownloadState(downloaded_pkg)
    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)

    # Trigger download and wait in new incognito window.
    self.DownloadAndWaitForStart(file_url, 1)
    self.WaitForAllDownloadsToComplete(1)
    # Remove next line when WaitForAllDownloadsToComplete can reliably wait
    # for downloads in incognito window. crbug.com/69738
    self.WaitForDownloadToComplete(downloaded_pkg)
    incognito_downloads = self.GetDownloadsInfo(1).Downloads()

    # Verify that download info exists in the correct profile.
    self.assertEqual(len(incognito_downloads), 1)
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg),
        msg='%s (size %d) and %s (size %d) do not match' % (
            file_path, os.path.getsize(file_path),
            downloaded_pkg, os.path.getsize(downloaded_pkg)))
    self.assertTrue(self.IsDownloadShelfVisible(1))

  def testSaveDangerousFile(self):
    """Verify that we can download and save a dangerous file."""
    file_path = self._GetDangerousDownload()
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    self._ClearLocalDownloadState(downloaded_pkg)
    self._TriggerUnsafeDownload(os.path.basename(file_path))
    self.PerformActionOnDownload(self._GetDownloadId(),
                                 'save_dangerous_download')
    self.WaitForDownloadToComplete(downloaded_pkg)

    # Verify that the file was downloaded.
    self.assertTrue(os.path.exists(downloaded_pkg))
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg))
    self._DeleteAfterShutdown(downloaded_pkg)

  def testDeclineDangerousDownload(self):
    """Verify that we can decline dangerous downloads"""
    file_path = self._GetDangerousDownload()
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    self._ClearLocalDownloadState(downloaded_pkg)
    self._TriggerUnsafeDownload(os.path.basename(file_path))
    self.PerformActionOnDownload(self._GetDownloadId(),
                                 'decline_dangerous_download')
    self.assertFalse(os.path.exists(downloaded_pkg))
    self.assertFalse(self.GetDownloadsInfo().Downloads())
    self.assertFalse(self.IsDownloadShelfVisible())

  def testRemoveDownload(self):
    """Verify that we can remove a download."""
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_path = os.path.join(test_dir, 'a_zip_file.zip')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    self._ClearLocalDownloadState(downloaded_pkg)

    self.DownloadAndWaitForStart(file_url)
    self.PerformActionOnDownload(self._GetDownloadId(), 'remove')

    # The download is removed from downloads, but not from the disk.
    self.assertFalse(self.GetDownloadsInfo().Downloads())
    self.assertTrue(os.path.exists(downloaded_pkg))
    self._DeleteAfterShutdown(downloaded_pkg)

  def testBigZip(self):
    """Verify that we can download a 1GB file.

    This test needs 2 GB of free space, 1 GB for the original zip file and
    another for the downloaded one.

    Note: This test increases automation timeout to 4 min.  Things might seem
          to hang.
    """
    # Create a 1 GB file on the fly
    file_path = self._MakeFile(2**30)
    self._DeleteAfterShutdown(file_path)
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    self._ClearLocalDownloadState(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    self._DeleteAfterShutdown(downloaded_pkg)
    # Waiting for big file to download might exceed automation timeout.
    # Temporarily increase the automation timeout.
    test_utils.CallFunctionWithNewTimeout(self, 4 * 60 * 1000,  # 4 min.
                                          self.WaitForAllDownloadsToComplete)
    # Verify that the file was correctly downloaded
    self.assertTrue(os.path.exists(downloaded_pkg),
                    'Downloaded file %s missing.' % downloaded_pkg)
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg),
                    'Downloaded file %s does not match original' %
                      downloaded_pkg)

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
      self._ClearLocalDownloadState(expected_filename)
      self.DownloadAndWaitForStart(file_url)

    self.WaitForAllDownloadsToComplete()

    # Verify that all files exist and have the right name
    for filename in renamed_files:
      self.assertTrue(os.path.exists(filename))
      self._DeleteAfterShutdown(filename)

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
    self._DeleteAfterShutdown(unicode(temp_dir))
    # Windows has a dual nature dealing with unicode filenames.
    # While the files are internally saved as unicode, there's a non-unicode
    # aware API that returns a locale-dependent coding on the true unicode
    # filenames.  This messes up things.
    # Filesystem-interfacing functions like os.listdir() need to
    # be given unicode strings to "do the right thing" on win.
    # Ref: http://boodebr.org/main/python/all-about-python-and-unicode
    for filename in crazy_filenames:  # filename is unicode.
      utf8_filename = filename.encode('utf-8')
      file_path = os.path.join(temp_dir, utf8_filename)
      _CreateFile(os.path.join(temp_dir, filename))  # unicode file.
      file_url = self.GetFileURLForPath(file_path)
      downloaded_file = os.path.join(download_dir, filename)
      self._ClearLocalDownloadState(downloaded_file)
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

  def _TriggerUnsafeDownload(self, filename, tab_index=0, windex=0):
    """Trigger download of an unsafe/dangerous filetype.

    Files explictly requested by the user (like navigating to a package, or
    clicking on a link) aren't marked unsafe.
    Only the ones where the user didn't directly initiate a download are
    marked unsafe.

    Navigates to download-dangerous.html which triggers the download.
    Waits until the download starts.

    Args:
      filename: the name of the file to trigger the download.
                This should exist in the 'dangerous' directory.
      tab_index: tab index. Default 0.
      windex: window index. Default 0.
    """
    dangerous_dir = os.path.join(
        self.DataDir(), 'downloads', 'dangerous')
    assert os.path.isfile(os.path.join(dangerous_dir, filename))
    file_url = self.GetFileURLForPath(os.path.join(
        dangerous_dir, 'download-dangerous.html')) + '?' + filename
    num_downloads = len(self.GetDownloadsInfo().Downloads())
    self.NavigateToURL(file_url, windex, tab_index)
    # It might take a while for the download to kick in, hold on until then.
    self.assertTrue(self.WaitUntil(
        lambda: len(self.GetDownloadsInfo().Downloads()) == num_downloads + 1))

  def testNoUnsafeDownloadsOnRestart(self):
    """Verify that unsafe file should not show up on session restart."""
    file_path = self._GetDangerousDownload()
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    self._ClearLocalDownloadState(downloaded_pkg)
    self._TriggerUnsafeDownload(os.path.basename(file_path))
    self.assertTrue(self.IsDownloadShelfVisible())
    current_downloads = self.GetDownloadsInfo().Downloads()
    # Restart the browser and assert that the download was removed.
    self.RestartBrowser(clear_profile=False)
    self.assertFalse(os.path.exists(downloaded_pkg))
    self.assertFalse(self.IsDownloadShelfVisible())
    self.NavigateToURL("chrome://downloads")
    new_downloads = self.GetDownloadsInfo().Downloads()
    if new_downloads:
      logging.info('Dangerous unconfirmed download survived restart.')
      logging.info('Old downloads list: %s' % current_downloads)
      logging.info('New downloads list: %s' % new_downloads)
    self.assertFalse(new_downloads)

  def testPauseAndResume(self):
    """Verify that pause and resume work while downloading a file.

    Note: This test increases automation timeout to 2 min.  Things might seem
          to hang.
    """
    # Create a 250 MB file on the fly
    file_path = self._MakeFile(2**28)

    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    self._ClearLocalDownloadState(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)

    self._DeleteAfterShutdown(downloaded_pkg)
    self._DeleteAfterShutdown(file_path)

    # Pause the download and assert that it is paused.
    pause_dict = self.PerformActionOnDownload(self._GetDownloadId(),
                                              'toggle_pause')
    if pause_dict['state'] == 'COMPLETE':
      logging.info('The download completed before pause. Stopping test.')
      return

    self.assertTrue(pause_dict['is_paused'])
    self.assertTrue(pause_dict['state'] == 'IN_PROGRESS')

    # Resume the download and assert it is not paused.
    resume_dict = self.PerformActionOnDownload(self._GetDownloadId(),
                                               'toggle_pause')
    self.assertFalse(resume_dict['is_paused'])

    # Waiting for big file to download might exceed automation timeout.
    # Temporarily increase the automation timeout.
    test_utils.CallFunctionWithNewTimeout(self, 2 * 60 * 1000,  # 2 min.
                                          self.WaitForAllDownloadsToComplete)

    # Verify that the file was correctly downloaded after pause and resume.
    self.assertTrue(os.path.exists(downloaded_pkg),
                    'Downloaded file %s missing.' % downloaded_pkg)
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg),
                    'Downloaded file %s does not match original' %
                    downloaded_pkg)

  def testCancelDownload(self):
    """Verify that we can cancel a download."""
    # Create a big file (250 MB) on the fly, so that the download won't finish
    # before being cancelled.
    file_path = self._MakeFile(2**28)
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    self._ClearLocalDownloadState(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    self.PerformActionOnDownload(self._GetDownloadId(), 'cancel')
    self._DeleteAfterShutdown(file_path)

    state = self.GetDownloadsInfo().Downloads()[0]['state']
    if state == 'COMPLETE':
      logging.info('The download completed before cancel. Test stopped.')
      return

    # Verify the download has been cancelled.
    self.assertEqual('CANCELLED',
                     self.GetDownloadsInfo().Downloads()[0]['state'])
    self.assertFalse(os.path.exists(downloaded_pkg))

  def testDownloadsPersistence(self):
    """Verify that download history persists on session restart."""
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_url = self.GetFileURLForPath(os.path.join(test_dir, 'a_zip_file.zip'))
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    self._ClearLocalDownloadState(downloaded_pkg)
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
    self._DeleteAfterShutdown(downloaded_pkg)

  def testDownloadTheme(self):
    """Verify downloading and saving a theme file installs the theme."""
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'extensions')
    file_url = self.GetFileURLForPath(os.path.join(test_dir, 'theme.crx'))
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'theme.crx')
    self._ClearLocalDownloadState(downloaded_pkg)

    self.DownloadAndWaitForStart(file_url)
    self.PerformActionOnDownload(self._GetDownloadId(),
                                 'save_dangerous_download')
    # Wait for the theme to be set automatically.
    self.assertTrue(self.WaitUntilDownloadedThemeSet('camo theme'))
    self.assertTrue(self.WaitUntil(lambda path: not os.path.exists(path),
                                   args=[downloaded_pkg]))

  def testAlwaysOpenFileType(self):
    """Verify "Always Open Files of this Type" download option

    If 'always open' option is set for any filetype, downloading that type of
    file gets opened always after the download.
    A cross-platform trick to verify it, by downloading a .zip file and
    expecting it to get unzipped.  Just check if it got unzipped or not.
    This way you won't have to worry about which application might 'open'
    it.
    """
    if not self.IsMac():
      logging.info('Don\'t have a standard way to test when a file "opened"')
      logging.info('Bailing out')
      return
    file_path = os.path.join(self.DataDir(), 'downloads', 'a_zip_file.zip')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    self.WaitForAllDownloadsToComplete()
    id = self._GetDownloadId()
    self.PerformActionOnDownload(id, 'toggle_open_files_like_this')
    # Retesting the flag we set
    file_url2 = self.GetFileURLForDataPath(os.path.join('zip', 'test.zip'))
    unzip_path = os.path.join(self.GetDownloadDirectory().value(),
                              'test', 'foo')
    os.path.exists(unzip_path) and pyauto_utils.RemovePath(unzip_path)
    self.DownloadAndWaitForStart(file_url2)
    self.WaitForAllDownloadsToComplete()
    # When the downloaded zip gets 'opened', a_file.txt will become available.
    self.assertTrue(self.WaitUntil(lambda: os.path.exists(unzip_path)),
                    'Did not open the filetype')
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    os.path.exists(unzip_path) and pyauto_utils.RemovePath(unzip_path)

  def testExtendedAttributesOnMac(self):
    """Verify that Chrome sets the extended attributes on a file.
       This test is for mac only.
    """
    if not self.IsMac():
      logging.info('Skipping testExtendedAttributesOnMac on non-Mac')
      return
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  'a_zip_file.zip')
    self._ClearLocalDownloadState(downloaded_pkg)
    file_url = 'http://src.chromium.org/viewvc/chrome/trunk/src/chrome/'\
               'test/data/downloads/a_zip_file.zip'
    self.DownloadAndWaitForStart(file_url)
    self.WaitForAllDownloadsToComplete()
    import xattr
    self.assertTrue('com.apple.quarantine' in xattr.listxattr(downloaded_pkg))

  def testOpenWhenDone(self):
    """Verify "Open When Done" download option.

    Test creates a zip file on the fly and downloads it.
    Set this option when file is downloading. Once file is downloaded,
    verify that downloaded zip file is unzipped.
    """
    if not self.IsMac():
      logging.info('Don\'t have a standard way to test when a file "opened"')
      logging.info('Bailing out')
      return
    # Creating a temp zip file.
    file_path = self._MakeFile(2**24)
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    id = self._GetDownloadId()
    self.PerformActionOnDownload(id, 'open')
    self.WaitForAllDownloadsToComplete()
    unzip_file_name = downloaded_pkg + '.cpgz'
    # Verify that the file was correctly downloaded.
    self.assertTrue(self.WaitUntil(lambda: os.path.exists(unzip_file_name)),
                    'Unzipped folder %s missing.' % unzip_file_name)
    self.assertTrue(os.path.exists(downloaded_pkg),
                    'Downloaded file %s missing.' % downloaded_pkg)
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg),
                    'Downloaded file %s does not match original' %
                      downloaded_pkg)
    os.path.exists(file_path) and os.remove(file_path)
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    os.path.exists(unzip_file_name) and os.remove(unzip_file_name)

  def testDownloadPercentage(self):
    """Verify that during downloading, % values increases,
       and once download is over, % value is 100"""
    file_path = self._MakeFile(2**24)
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)
    self.DownloadAndWaitForStart(file_url)
    downloaded_pkg = os.path.join(self.GetDownloadDirectory().value(),
                                  os.path.basename(file_path))
    downloads = self.GetDownloadsInfo().Downloads()
    old_percentage = downloads[0]['PercentComplete']
    def _PercentInc():
      percent = self.GetDownloadsInfo().Downloads()[0]['PercentComplete']
      return old_percentage == 100 or percent > old_percentage,
    self.assertTrue(self.WaitUntil(_PercentInc),
        msg='Download percentage value is not increasing')
    # Once download is completed, percentage is 100.
    self.WaitForAllDownloadsToComplete()
    downloads = self.GetDownloadsInfo().Downloads()
    self.assertEqual(downloads[0]['PercentComplete'], 100,
        'Download percentage should be 100 after download completed')
    os.path.exists(file_path) and os.remove(file_path)
    os.path.exists(downloaded_pkg) and os.remove(downloaded_pkg)

  def testDownloadIncognitoAndRegular(self):
    """Download the same zip file in regular and incognito window and
       verify that it downloaded correctly with same file name appended with
       counter for the second download in regular window.
    """
    test_dir = os.path.join(os.path.abspath(self.DataDir()), 'downloads')
    file_path = os.path.join(test_dir, 'a_zip_file.zip')
    file_url = self.GetFileURLForPath(file_path)
    downloaded_pkg_regul = os.path.join(self.GetDownloadDirectory().value(),
                                        'a_zip_file.zip')
    downloaded_pkg_incog = os.path.join(self.GetDownloadDirectory().value(),
                                        'a_zip_file (1).zip')
    self._ClearLocalDownloadState(downloaded_pkg_regul)
    self._ClearLocalDownloadState(downloaded_pkg_incog)

    self.DownloadAndWaitForStart(file_url, 0)
    self.WaitForAllDownloadsToComplete(0)

    self.RunCommand(pyauto.IDC_NEW_INCOGNITO_WINDOW)
    self.DownloadAndWaitForStart(file_url, 1)
    self.WaitForAllDownloadsToComplete(1)

    # Verify download in regular window.
    self.assertTrue(os.path.exists(downloaded_pkg_regul))
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg_regul))

    # Verify download in incognito window.
    # bug 69738 WaitForAllDownloadsToComplete is flaky for this test case.
    # Using extra WaitUntil until this is resolved.
    self.assertTrue(self.WaitUntil(
        lambda: os.path.exists(downloaded_pkg_incog)))
    self.assertTrue(self._EqualFileContents(file_path, downloaded_pkg_incog))


if __name__ == '__main__':
  pyauto_functional.Main()
