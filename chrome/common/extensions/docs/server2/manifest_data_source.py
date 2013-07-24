# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import deepcopy
from operator import itemgetter

from third_party.json_schema_compiler.json_parse import OrderedDict, Parse

class ManifestDataSource(object):
  '''Provides a template with access to manifest properties specific to apps or
  extensions.
  '''
  def __init__(self,
               compiled_fs_factory,
               file_system,
               manifest_path,
               features_path):
    self._manifest_path = manifest_path
    self._features_path = features_path
    self._file_system = file_system
    self._cache = compiled_fs_factory.Create(
        self._CreateManifestData, ManifestDataSource)

  def _ApplyAppsTransformations(self, manifest):
    manifest['required'][0]['example'] = 'Application'
    manifest['optional'][-1]['is_last'] = True

  def _ApplyExtensionsTransformations(self, manifest):
    manifest['optional'][-1]['is_last'] = True

  def _CreateManifestData(self, _, content):
    '''Take the contents of |_manifest_path| and create apps and extensions
    versions of a manifest example based on the contents of |_features_path|.
    '''
    def create_manifest_dict():
      manifest_dict = OrderedDict()
      for category in ('required', 'only_one', 'recommended', 'optional'):
        manifest_dict[category] = []
      return manifest_dict

    apps = create_manifest_dict()
    extensions = create_manifest_dict()

    manifest_json = Parse(content)
    features_json = Parse(self._file_system.ReadSingle(
        self._features_path))

    def add_property(feature, manifest_key, category):
      '''If |feature|, from features_json, has the correct extension_types, add
      |manifest_key| to either apps or extensions.
      '''
      added = False
      extension_types = feature['extension_types']
      if extension_types == 'all' or 'platform_app' in extension_types:
        apps[category].append(deepcopy(manifest_key))
        added = True
      if extension_types == 'all' or 'extension' in extension_types:
        extensions[category].append(deepcopy(manifest_key))
        added = True
      return added

    # Property types are: required, only_one, recommended, and optional.
    for category in manifest_json:
      for manifest_key in manifest_json[category]:
        # If a property is in manifest.json but not _manifest_features, this
        # will cause an error.
        feature = features_json[manifest_key['name']]
        if add_property(feature, manifest_key, category):
          del features_json[manifest_key['name']]

    # All of the properties left in features_json are assumed to be optional.
    for feature in features_json.keys():
      item = features_json[feature]
      # Handles instances where a features entry is a union with a whitelist.
      if isinstance(item, list):
        item = item[0]
      add_property(item, {'name': feature}, 'optional')

    apps['optional'].sort(key=itemgetter('name'))
    extensions['optional'].sort(key=itemgetter('name'))

    self._ApplyAppsTransformations(apps)
    self._ApplyExtensionsTransformations(extensions)

    return {'apps': apps, 'extensions': extensions}

  def get(self, key):
    return self._cache.GetFromFile(self._manifest_path)[key]
