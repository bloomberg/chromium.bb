#!/usr/bin/python

# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

import pyauto_functional
import pyauto_utils


"""Commonly used functions for PyAuto tests."""

def DownloadFileFromDownloadsDataDir(test, file_name):
  """Download a file from downloads data directory.

  Args:
    test: derived from pyauto.PyUITest - base class for UI test cases
    file_name: name of file to download
  """
  download_dir = os.path.join(os.path.abspath(test.DataDir()), 'downloads')
  file_path = os.path.join(download_dir, file_name)
  file_url = test.GetFileURLForPath(file_path)
  downloaded_pkg = os.path.join(test.GetDownloadDirectory().value(),
                                file_name)
  # Check if file already exists. If so then delete it.
  if os.path.exists(downloaded_pkg):
    RemoveDownloadedTestFile(test, file_name)
  test.DownloadAndWaitForStart(file_url)
  test.WaitForAllDownloadsToComplete()


def RemoveDownloadedTestFile(test, file_name):
  """Delete a file from the downloads directory.

  Arg:
    test: derived from pyauto.PyUITest - base class for UI test cases
    file_name: name of file to remove
  """
  downloaded_pkg = os.path.join(test.GetDownloadDirectory().value(),
                                file_name)
  pyauto_utils.RemovePath(downloaded_pkg)
  pyauto_utils.RemovePath(downloaded_pkg + '.crdownload')
