# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import codecs
import socket

from common import url_helper
from slave.gtest.networktransaction import NetworkTransaction


class TestResultsUploader(object):
  def __init__(self, host):
    self._host = host

  def _upload_files(self, attrs, file_objs):
    url = "http://%s/testfile/upload" % self._host
    url_helper.upload_files(url, attrs, file_objs)

  def upload(self, params, files, timeout_seconds):
    file_objs = []
    for filename, path in files:
      with codecs.open(path, "rb") as f:
        file_objs.append(('file', filename, f.read()))

    orig_timeout = socket.getdefaulttimeout()
    try:
      socket.setdefaulttimeout(timeout_seconds)
      NetworkTransaction(timeout_seconds=timeout_seconds).run(
        lambda: self._upload_files(params, file_objs))
    finally:
      socket.setdefaulttimeout(orig_timeout)
