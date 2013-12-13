# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from operator import itemgetter

import docs_server_utils as utils

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
                 availability_finder,
                 api_categorizer):
      self._file_system = file_system
      self._features_bundle = features_bundle
      self._api_categorizer = api_categorizer
      self._object_store_creator = object_store_creator
      self._api_models = api_models
      self._availability_finder = availability_finder

    def _GenerateAPIDict(self):

      def _GetChannelInfo(api_name):
        return self._availability_finder.GetApiAvailability(api_name)

      def _GetApiPlatform(api_name):
        feature = self._features_bundle.GetAPIFeatures().Get()[api_name]
        return feature['platforms']

      def _MakeDictForPlatform(platform):
        platform_dict = {
          'chrome': {'stable': [], 'beta': [], 'dev': [], 'trunk': []},
        }
        private_apis = []
        experimental_apis = []
        all_apis = []
        for api_name, api_model in self._api_models.IterModels():
          if not self._api_categorizer.IsDocumented(platform, api_name):
            continue
          api = {
            'name': api_name,
            'description': api_model.description,
            'platforms': _GetApiPlatform(api_name),
          }
          category = self._api_categorizer.GetCategory(platform, api_name)
          if category == 'chrome':
            channel_info = _GetChannelInfo(api_name)
            channel = channel_info.channel
            if channel == 'stable':
              version = channel_info.version
              api['version'] = version
            platform_dict[category][channel].append(api)
            all_apis.append(api)
          elif category == 'experimental':
            experimental_apis.append(api)
            all_apis.append(api)
          elif category == 'private':
            private_apis.append(api)

        for channel, apis_by_channel in platform_dict['chrome'].iteritems():
          apis_by_channel.sort(key=itemgetter('name'))
          utils.MarkLast(apis_by_channel)
          platform_dict['chrome'][channel] = apis_by_channel

        for key, apis in (('all', all_apis),
                          ('private', private_apis),
                          ('experimental', experimental_apis)):
          apis.sort(key=itemgetter('name'))
          utils.MarkLast(apis)
          platform_dict[key] = apis

        return platform_dict
      return {
        'apps': _MakeDictForPlatform('apps'),
        'extensions': _MakeDictForPlatform('extensions'),
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
