# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""DownloadInfo: python representation for downloads visible to Chrome.

Obtain one of these from PyUITestSuite::GetDownloadsInfo() call.

class MyDownloadsTest(pyauto.PyUITest):
  def testDownload(self):
    self.DownloadAndWaitForStart('http://my.url/package.zip')
    self.WaitForAllDownloadsToComplete()
    info = self.GetDownloadsInfo()
    print info.Downloads()
    self.assertEqual(info.Downloads()[0]['file_name'], 'packge.zip')

See more tests in chrome/test/functional/downloads.py.
"""

import os
import simplejson as json
import sys

from pyauto_errors import JSONInterfaceError


class DownloadInfo(object):
  """Represent info about Downloads.

  The info is represented as a list of DownloadItems.  Each DownloadItem is a
  dictionary with various attributes about a download, like id, file_name,
  path, state, and so on.
  """
  def __init__(self, json_string):
    """Initialize a DownloadInfo from a string of json.

    Args:
      json_string: a string of JSON, as returned by a json ipc call for the
                   command 'GetDownloadsInfo'
                   A typical json string representing one download looks like:
                   {'downloads': [{'url': 'http://blah/a_file.zip',
                                   'file_name': 'a_file.zip',
                                   'state': 'COMPLETED',
                                   ...,
                                   ..., } ] }

    Raises:
      pyauto_errors.JSONInterfaceError if the automation call returns an error.
    """
    # JSON string prepared in GetDownloadsInfo() in automation_provider.cc
    self.downloadsdict = json.loads(json_string)
    if self.downloadsdict.has_key('error'):
      raise JSONInterfaceError(self.downloadsdict['error'])

  def Downloads(self):
    """Info about all downloads.

    This includes downloads in all states (COMPLETE, IN_PROGRESS, ...).

    Returns:
      [downloaditem1, downloaditem2, ...]
    """
    return self.downloadsdict.get('downloads', [])

  def DownloadsInProgress(self):
    """Info about all downloads in progress.

    Returns:
      [downloaditem1, downloaditem2, ...]
    """
    return [x for x in self.Downloads() if x['state'] == 'IN_PROGRESS']

  def DownloadsComplete(self):
    """Info about all downloads that have completed.

    Returns:
      [downloaditem1, downloaditem2, ...]
    """
    return [x for x in self.Downloads() if x['state'] == 'COMPLETE']
