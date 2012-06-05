# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import time

from third_party.handlebar import Handlebar

# Cache templates for 5 minutes.
CACHE_TIMEOUT = 300

# Cached templates stored as (branch, path) -> (template, cache time)
# The template cache is global so it will not be erased each time a new
# TemplateFetcher is created.
TEMPLATE_CACHE = {}

class TemplateFetcher(object):
  def __init__(self, branch, fetcher):
    self._fetcher = fetcher
    self._branch = branch

  def __getitem__(self, path):
    key = (self._branch, path)
    if key in TEMPLATE_CACHE:
      compiled_template, compile_time = TEMPLATE_CACHE[key]
      if (time.clock() - compile_time) > CACHE_TIMEOUT:
        TEMPLATE_CACHE.pop(key)
    if key not in TEMPLATE_CACHE:
      logging.info('Template cache miss for: ' + path)
      try:
        template = self._fetcher.FetchResource(self._branch, path).content
        compiled_template = Handlebar(template)
        TEMPLATE_CACHE[key] = (compiled_template, time.clock())
      except:
        return ''

    return compiled_template
