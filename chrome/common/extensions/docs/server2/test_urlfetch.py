# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

def _ReadFile(filename):
  with open(filename, 'r') as f:
    return f.read()

class _MockResponse(object):
  def __init__(self):
    self.content = ''
    self.headers = { 'content-type': 'none' }

def fetch(url):
  result = _MockResponse()
  # If a test needs to go into a directory that needs to have the same name as
  # a file, such as the recursive directory listing subversion_fetcher_test,
  # remove all .html's not at the end of the path.
  url = url[:-5].replace('.html', '') + url[-5:]
  result.content = _ReadFile(os.path.join('test_data', url))
  return result
