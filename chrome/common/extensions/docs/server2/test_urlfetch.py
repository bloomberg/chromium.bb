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

def fetch(url):
  result = _MockResponse()
  result.content = _ReadFile(os.path.join('test_data', url))
  return result
