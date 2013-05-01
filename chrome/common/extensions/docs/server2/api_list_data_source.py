# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os

import third_party.json_schema_compiler.model as model
import docs_server_utils as utils

class APIListDataSource(object):
  """ This class creates a list of chrome.* APIs and chrome.experimental.* APIs
  for extensions and apps that are used in the api_index.html and
  experimental.html pages.
  |api_path| is the path to the API schemas.
  |public_path| is the path to the public HTML templates.
  An API is considered listable if it's in both |api_path| and |public_path| -
  the API schemas may contain undocumentable APIs, and the public HTML templates
  will contain non-API articles.
  """
  class Factory(object):
    def __init__(self, compiled_fs_factory, api_path, public_path):
      self._compiled_fs = compiled_fs_factory.Create(self._ListAPIs,
                                                     APIListDataSource)
      self._identity_fs = compiled_fs_factory.CreateIdentity(APIListDataSource)
      def Normalize(string):
        return string if string.endswith('/') else (string + '/')
      self._api_path = Normalize(api_path)
      self._public_path = Normalize(public_path)

    def _GetAPIsInSubdirectory(self, api_names, doc_type):
      public_templates = self._identity_fs.GetFromFileListing(
          '%s%s/' % (self._public_path, doc_type))
      template_names = set(os.path.splitext(name)[0]
                           for name in public_templates)
      experimental_apis = []
      chrome_apis = []
      for template_name in sorted(template_names):
        if model.UnixName(template_name) not in api_names:
          continue
        entry = {'name': template_name.replace('_', '.')}
        if template_name.startswith('experimental'):
          experimental_apis.append(entry)
        else:
          chrome_apis.append(entry)
      if len(chrome_apis):
        chrome_apis[-1]['last'] = True
      if len(experimental_apis):
        experimental_apis[-1]['last'] = True
      return {
        'chrome': chrome_apis,
        'experimental': experimental_apis
      }

    def _ListAPIs(self, base_dir, apis):
      api_names = set(utils.SanitizeAPIName(name) for name in apis)
      return {
        'apps': self._GetAPIsInSubdirectory(api_names, 'apps'),
        'extensions': self._GetAPIsInSubdirectory(api_names, 'extensions')
      }

    def Create(self):
      return APIListDataSource(self._compiled_fs, self._api_path)

  def __init__(self, compiled_fs, api_path):
    self._compiled_fs = compiled_fs
    self._api_path = api_path

  def GetAllNames(self):
    names = []
    for platform in ['apps', 'extensions']:
      for category in ['chrome', 'experimental']:
       names.extend(self.get(platform).get(category))
    return [api_name['name'] for api_name in names]

  def get(self, key):
    return self._compiled_fs.GetFromFileListing(self._api_path)[key]
