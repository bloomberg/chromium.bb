# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os
import time

from third_party.handlebar import Handlebar

class TemplateDataSource(object):
  def __init__(self, fetcher, base_paths, cache_timeout_seconds):
    logging.info('Template data source created: %s %d' %
        (' '.join(base_paths), cache_timeout_seconds))
    self._fetcher = fetcher
    self._template_cache = {}
    self._base_paths = base_paths
    self._cache_timeout_seconds = cache_timeout_seconds

  def Render(self, template_name, context):
    """This method will render a template named |template_name|, fetching all
    the partial templates needed with |self._fetcher|. Partials are retrieved
    from the TemplateDataSource with the |get| method.
    """
    template = self.get(template_name)
    if not template:
      return ''
      # TODO error handling
    return template.render(json.loads(context), {'templates': self}).text

  class _CachedTemplate(object):
    def __init__(self, template, expiry):
      self.template = template
      self._expiry = expiry

    def HasExpired(self):
      return time.time() > self._expiry

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    index = key.rfind('.html')
    if index > 0:
      key = key[:index]
    path = key + '.html'
    if key in self._template_cache:
      if self._template_cache[key].HasExpired():
        self._template_cache.pop(key)
    if key not in self._template_cache:
      logging.info('Template cache miss for: ' + path)
      compiled_template = None
      for base_path in self._base_paths:
        try:
          template = self._fetcher.FetchResource(base_path + path).content
          compiled_template = Handlebar(template)
          self._template_cache[key] = self._CachedTemplate(
              compiled_template,
              time.time() + self._cache_timeout_seconds)
          break
        except:
          pass

    return compiled_template
