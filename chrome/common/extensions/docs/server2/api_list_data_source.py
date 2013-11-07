# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from operator import itemgetter
import os
import posixpath

from svn_constants import PUBLIC_TEMPLATE_PATH
import docs_server_utils as utils

def _GetAPICategory(api, documented_apis):
  name = api['name']
  if (name.endswith('Private') or
      name not in documented_apis):
    return 'private'
  if name.startswith('experimental.'):
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
                 object_store_creator):
      self._file_system = file_system
      self._cache = compiled_fs_factory.Create(file_system,
                                               self._CollectDocumentedAPIs,
                                               APIListDataSource)
      self._features_bundle = features_bundle
      self._object_store_creator = object_store_creator

    def _CollectDocumentedAPIs(self, base_dir, files):
      def GetDocumentedAPIsForPlatform(names, platform):
        public_templates = []
        for root, _, files in self._file_system.Walk(posixpath.join(
            PUBLIC_TEMPLATE_PATH, platform)):
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
      documented_apis = self._cache.GetFromFileListing(
          PUBLIC_TEMPLATE_PATH).Get()
      api_features = self._features_bundle.GetAPIFeatures().Get()

      def FilterAPIs(platform):
        return (api for api in api_features.itervalues()
            if platform in api['platforms'])

      def MakeDictForPlatform(platform):
        platform_dict = { 'chrome': [], 'experimental': [], 'private': [] }
        for api in FilterAPIs(platform):
          if api['name'] in documented_apis[platform]:
            category = _GetAPICategory(api, documented_apis[platform])
            platform_dict[category].append(api)
        for category, apis in platform_dict.iteritems():
          platform_dict[category] = sorted(apis, key=itemgetter('name'))
          utils.MarkLast(platform_dict[category])
        return platform_dict

      return {
        'apps': MakeDictForPlatform('apps'),
        'extensions': MakeDictForPlatform('extensions')
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
