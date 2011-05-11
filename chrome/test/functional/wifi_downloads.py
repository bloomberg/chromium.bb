#!/usr/bin/python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import hashlib
import logging
import os
import time

import pyauto_functional  # Must be imported before pyauto
import pyauto
import pyauto_utils
import chromeos_network


class WifiDownloadsTest(chromeos_network.PyNetworkUITest):
  """TestCase for ChromeOS Wifi Downloads

     This test makes a few assumptions.  It needs to have access to the power
     strip used in pyautolib/chromeos/wifi_downloads.py.  It also assumes access
     to the server 172.22.12.98:8080.  If the server is passed a filname in the
     format <integer>.lf, it will generate a file of size <integer> in KB.  In
     addition the name of the file returned is the md5 checksum of the file.
  """

  def setUp(self):
    chromeos_network.PyNetworkUITest.setUp(self)
    self.InitWifiPowerStrip()
    # The power strip is a shared resource and if we every crash in the middle
    # of a test we will be in an unknown state.  This returns us to 'all off'.
    self.PowerDownAllRouters()
    # Downloading files of a large size, can take a while, bump the timeout
    self.changer = pyauto.PyUITest.ActionTimeoutChanger(self, 4 * 1000 * 60)

  def _Md5Checksum(self, file_path):
    """Returns the md5 checksum of a file at a given path.

    Args:
      file_path: The complete path of the file to generate the md5 checksum for.
    """
    file_handle = open(file_path, 'rb')
    m = hashlib.md5()
    while True:
      data = file_handle.read(8192)
      if not data:
        break
      m.update(data)
    file_handle.close()
    return m.hexdigest()

  def _ConnectToRouterAndVerify(self, router_name):
    """Generic routine for connecting to a router.

    Args:
      router_name: The name of the router to connect to.
    """
    router = self.GetRouterConfig(router_name)
    self.RouterPower(router_name, True)

    self.assertTrue(self.WaitUntilWifiNetworkAvailable(router['ssid']),
                    'Wifi network %s never showed up.' % router['ssid'])

    # Verify connect did not have any errors.
    error = self.ConnectToWifiRouter(router_name)
    self.assertFalse(error, 'Failed to connect to wifi network %s. '
                            'Reason: %s.' % (router['ssid'], error))

    # Verify the network we connected to.
    ssid = self.GetConnectedWifi()
    self.assertEqual(ssid, router['ssid'],
                     'Did not successfully connect to wifi network %s.' % ssid)

  def _DownloadAndVerifyFile(self, download_url):
    """Downloads a file at a given URL and validates it

    This method downloads a file from a server whose filename matches the md5
    checksum.  Then we manually generate the md5 and check it against the
    filename.

    Args:
      download_url: URL of the file to download.
    """
    start = time.time()
    # Make a copy of the download directory now to work around segfault
    downloads_dir = self.GetDownloadDirectory().value()
    self.DownloadAndWaitForStart(download_url)
    self.WaitForAllDownloadsToComplete()
    end = time.time()
    logging.info('Download took %2.2f seconds to complete' % (end - start))
    downloaded_files = os.listdir(downloads_dir)
    self.assertEqual(len(downloaded_files), 1, msg='Expected only one file in '
                    'the Downloads folder, there are more.')
    self.assertFalse(len(downloaded_files) == 0, msg='Expected only one file in'
                    ' the Downloads folder, there are none.')
    filename = os.path.splitext(downloaded_files[0])[0]
    file_path = os.path.join(self.GetDownloadDirectory().value(),
                                  downloaded_files[0])
    md5_sum = self._Md5Checksum(file_path)
    self.assertEqual(filename, md5_sum, 'The checksums do not match.  The '
                     'download is incomplete.')

  def testDownload1MBFile(self):
    """Test downloading a 1MB file from a wireless router."""
    download_url = 'http://172.22.12.98:8080/1024.lf'
    self._ConnectToRouterAndVerify('Nfiniti')
    self._DownloadAndVerifyFile(download_url)
    self.DisconnectFromWifiNetwork()

  def testDownload10MBFile(self):
    """Test downloading a 10MB file from a wireless router."""
    download_url = 'http://172.22.12.98:8080/10240.lf'
    self._ConnectToRouterAndVerify('Belkin_N+')
    self._DownloadAndVerifyFile(download_url)
    self.DisconnectFromWifiNetwork()

  def testDownload100MBFile(self):
    """Test downloading a 100MB file from a wireless router."""
    download_url = 'http://172.22.12.98:8080/102400.lf'
    self._ConnectToRouterAndVerify('Trendnet_639gr')
    self._DownloadAndVerifyFile(download_url)
    self.DisconnectFromWifiNetwork()


if __name__ == '__main__':
  pyauto_functional.Main()
