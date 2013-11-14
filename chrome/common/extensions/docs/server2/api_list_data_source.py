# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from operator import itemgetter
import os
import posixpath

import docs_server_utils as utils
from branch_utility import ChannelInfo
from extensions_paths import PUBLIC_TEMPLATES
from file_system import FileNotFoundError

def _GetAPICategory(api_name, documented_apis):
  if (api_name.endswith('Private') or
      api_name not in documented_apis):
    return 'private'
  if api_name.startswith('experimental.'):
    return 'experimental'
  return 'chrome'


class APIListDataSource(object):
  """ This class creates a list of chrome.* APIs and chrome.experimental.* APIs
  for extensions and apps that are used in the api_index.html,
  experimental.html, and private_apis.html pages.

  An API is considered listable if it is listed in _api_features.json,
  it has a corresponding HTML file in the public template path, and one of
  the following conditions is met:
    - It has no "dependencies" or "extension_types" properties in _api_features
    - It has an "extension_types" property in _api_features with either/both
      "extension"/"platform_app" values present.
    - It has a dependency in _{api,manifest,permission}_features with an
      "extension_types" property where either/both "extension"/"platform_app"
      values are present.
  """
  class Factory(object):
    def __init__(self,
                 compiled_fs_factory,
                 file_system,
                 features_bundle,
                 object_store_creator,
                 api_models,
                 availability_finder):
      self._file_system = file_system
      self._cache = compiled_fs_factory.Create(file_system,
                                               self._CollectDocumentedAPIs,
                                               APIListDataSource)
      self._features_bundle = features_bundle
      self._object_store_creator = object_store_creator
      self._api_models = api_models
      self._availability_finder = availability_finder

    def _GetDocumentedApis(self):
      return self._cache.GetFromFileListing(self._public_template_path).Get()

    def _CollectDocumentedAPIs(self, base_dir, files):
      def GetDocumentedAPIsForPlatform(names, platform):
        public_templates = []
        for root, _, files in self._file_system.Walk(posixpath.join(
            PUBLIC_TEMPLATES, platform)):
          public_templates.extend(
              ('%s/%s' % (root, name)).lstrip('/') for name in files)
        template_names = set(os.path.splitext(name)[0]
                             for name in public_templates)
        return [name.replace('_', '.') for name in template_names]
      api_names = set(utils.SanitizeAPIName(name) for name in files)
      return {
        'apps': GetDocumentedAPIsForPlatform(api_names, 'apps'),
        'extensions': GetDocumentedAPIsForPlatform(api_names, 'extensions')
      }

    def _GenerateAPIDict(self):

      documented_apis = self._cache.GetFromFileListing(PUBLIC_TEMPLATES).Get()

      def GetChannelInfo(api_name):
        return self._availability_finder.GetApiAvailability(api_name)

      def GetApiDescription(model):
        try:
          return model.Get().description if model.Get() else ''
        except FileNotFoundError:
          return ''

      def GetApiPlatform(api_name):
        feature = self._features_bundle.GetAPIFeatures().Get()[api_name]
        return feature['platforms']

      def MakeDictForPlatform(platform):
        platform_dict = {
          'chrome': {'stable': [], 'beta': [], 'dev': [], 'trunk': []}
        }
        private_apis = []
        experimental_apis = []
        all_apis = []
        for api_name, api_model in self._api_models.IterModels():
          api = {
            'name': api_name,
            'description': GetApiDescription(api_model),
            'platforms': GetApiPlatform(api_name),
          }
          category = _GetAPICategory(api_name, documented_apis[platform])
          if category == 'chrome':
            channel_info = GetChannelInfo(api_name)
            channel = channel_info.channel
            if channel == 'stable':
              api['version'] = channel_info.version
            platform_dict[category][channel].append(api)
            all_apis.append(api)
          elif category == 'experimental':
            experimental_apis.append(api)
            all_apis.append(api)
          elif category == 'private':
            private_apis.append(api)

        for channel, apis_by_channel in platform_dict['chrome'].iteritems():
          apis_by_channel = sorted(apis_by_channel, key=itemgetter('name'))
          utils.MarkLast(apis_by_channel)
          platform_dict['chrome'][channel] = apis_by_channel

        for key, apis in (('all', all_apis),
                          ('private', private_apis),
                          ('experimental', experimental_apis)):
          apis = sorted(apis, key=itemgetter('name'))
          utils.MarkLast(apis)
          platform_dict[key] = apis
        return platform_dict
      return {
        'apps': MakeDictForPlatform('apps'),
        'extensions': MakeDictForPlatform('extensions'),
      }

    def Create(self):
      return APIListDataSource(self, self._object_store_creator)

  def __init__(self, factory, object_store_creator):
    self._factory = factory
    self._object_store = object_store_creator.Create(APIListDataSource)

  def _GetCachedAPIData(self):
    data = self._object_store.Get('api_data').Get()
    if data is None:
      data = self._factory._GenerateAPIDict()
      self._object_store.Set('api_data', data)
    return data

  def get(self, key):
    return self._GetCachedAPIData().get(key)
