# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import time
import urllib2


class NetworkTimeout(Exception):
  pass


class NetworkTransaction(object):
  def __init__(self, initial_backoff_seconds=10, grown_factor=1.5,
               timeout_seconds=(10 * 60), convert_404_to_None=False):
    self._initial_backoff_seconds = initial_backoff_seconds
    self._backoff_seconds = initial_backoff_seconds
    self._grown_factor = grown_factor
    self._timeout_seconds = timeout_seconds
    self._convert_404_to_None = convert_404_to_None
    self._total_sleep = 0

  def run(self, request):
    self._total_sleep = 0
    self._backoff_seconds = self._initial_backoff_seconds
    while True:
      try:
        return request()
      except urllib2.HTTPError, e:
        if self._convert_404_to_None and e.code == 404:
          return None
        self._check_for_timeout()
        logging.warn("Received HTTP status %s loading \"%s\".  "
                    "Retrying in %s seconds...",
                    e.code, e.filename, self._backoff_seconds)
        self._sleep()

  def _check_for_timeout(self):
    if self._total_sleep + self._backoff_seconds > self._timeout_seconds:
      raise NetworkTimeout()

  def _sleep(self):
    time.sleep(self._backoff_seconds)
    self._total_sleep += self._backoff_seconds
    self._backoff_seconds *= self._grown_factor
