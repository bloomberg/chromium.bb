# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

from file_system import FileNotFoundError
import third_party.json_schema_compiler.model as model
from docs_server_utils import SanitizeAPIName

# These files are special cases that shouldn't be in the API list.
IGNORED_FILES = [
  'devtools'
]

class APIListDataSource(object):
  """ This class creates a list of chrome.* APIs and chrome.experimental.* APIs
  that are used in the api_index.html and experimental.html pages.
  """
  def __init__(self, cache_builder, file_system, api_path, public_path):
    self._cache = cache_builder.build(self._ListAPIs)
    self._file_system = file_system
    self._api_path = api_path + '/'
    self._public_path = public_path + '/'

  def _GetAPIsInSubdirectory(self, api_names, doc_type):
    public_templates = self._file_system.ReadSingle(
        self._public_path + doc_type + '/')
    template_names = [os.path.splitext(name)[0]
                      for name in public_templates]
    experimental_apis = []
    chrome_apis = []
    for template_name in sorted(template_names):
      if template_name in IGNORED_FILES:
        continue
      if model.UnixName(template_name) in api_names:
        if template_name.startswith('experimental'):
          experimental_apis.append({
            'name': template_name.replace('_', '.')
          })
        else:
          chrome_apis.append({ 'name': template_name.replace('_', '.') })
    chrome_apis[-1]['last'] = True
    experimental_apis[-1]['last'] = True
    return {
      'chrome': chrome_apis,
      'experimental': experimental_apis
    }

  def _ListAPIs(self, apis):
    api_names = set(SanitizeAPIName(name, self._api_path) for name in apis)
    return {
      'apps': self._GetAPIsInSubdirectory(api_names, 'apps'),
      'extensions': self._GetAPIsInSubdirectory(api_names, 'extensions')
    }

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    try:
      return self._cache.GetFromFileListing(self._api_path)[key]
    except FileNotFoundError as e:
      raise ValueError(str(e) + ': Error listing files for "%s".' % key)
