#!/usr/bin/python2.4
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""Download a file from a URL to a file on disk.

This module supports username and password with basic authentication.
"""

import base64
import os.path
import sys
import urllib2

import download_utils


def _CreateDirectory(path):
  """Create a directory tree, ignore if it's already there."""
  try:
    os.makedirs(path)
    return True
  except os.error:
    return False


def HttpDownload(url, target, username=None, password=None):
  """Download a file from a remote server.

  Args:
    url: A URL to download from.
    target: Filename to write download to.
    username: Optional username for download.
    password: Optional password for download (ignored if no username).
  """

  headers = [('Accept', '*/*')]
  if username:
    if password:
      auth_code = base64.b64encode(username + ':' + password)
    else:
      auth_code = base64.b64encode(username)
    headers.append(('Authorization', 'Basic ' + auth_code))
  opener = urllib2.build_opener()
  opener.addheaders = headers
  urllib2.install_opener(opener)
  _CreateDirectory(os.path.split(target)[0])
  # Retry up to 10 times (appengine logger is flaky).
  retries = 10
  for i in range(retries):
    try:
      src = urllib2.urlopen(url)
      try:
        download_utils.WriteDataFromStream(target, src, chunk_size=2**20,
                                           verbose=True)
      finally:
        src.close()
      break
    except urllib2.HTTPError:
      if i < retries - 1:
        sys.stdout.write('Download failed on %s, retrying... (%d)\n' % (url, i))
        continue
      sys.stdout.write('Download failed on %s, giving up.\n' % url)
      raise
