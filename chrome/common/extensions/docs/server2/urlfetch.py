# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from google.appengine.api import urlfetch
from google.appengine.api import memcache

DEFAULT_CACHE_TIME = 300

class _FetchException(Exception):
  """Thrown when status code is not 200.
  """
  def __init__(self, url):
    Exception.__init__(self, 'Fetch exception from ' + url)

def fetch(url):
  result = memcache.get(url, namespace=__name__)
  if result is not None:
    return result

  result = urlfetch.fetch(url)
  if result.status_code != 200:
    raise _FetchException(url)

  memcache.add(url, result, DEFAULT_CACHE_TIME, namespace=__name__)
  return result
