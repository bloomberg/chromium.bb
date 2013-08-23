# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from itertools import ifilter
from operator import itemgetter

import features_utility as features
from third_party.json_schema_compiler.json_parse import Parse

def _ListifyPermissions(permissions):
  '''Filter out any permissions that do not have a description or with a name
  that ends with Private then sort permissions features by name into a list.
  '''
  def filter_permissions(perm):
    return 'description' in perm and not perm['name'].endswith('Private')

  return sorted(
      ifilter(filter_permissions, permissions.values()),
      key=itemgetter('name'))

def _AddDependencyDescriptions(permissions, api_features):
  '''Use |api_features| to determine the dependencies APIs have on permissions.
  Add descriptions to |permissions| based on those dependencies.
  '''
  for name, permission in permissions.iteritems():
    # Don't overwrite the description created by expanding a partial template.
    if 'description' in permission or not permission['platforms']:
      continue

    has_deps = False
    if name in api_features:
      for dependency in api_features[name].get('dependencies', ()):
        if dependency.startswith('permission:'):
          has_deps = True

    if has_deps:
      permission['partial'] = 'permissions/generic_description'

class PermissionsDataSource(object):
  '''Load and format permissions features to be used by templates. Requries a
  template data source be set before use.
  '''
  def __init__(self,
               compiled_fs_factory,
               file_system,
               api_features_path,
               permissions_features_path,
               permissions_json_path):
    self._api_features_path = api_features_path
    self._permissions_features_path = permissions_features_path
    self._permissions_json_path = permissions_json_path
    self._file_system = file_system
    self._cache = compiled_fs_factory.Create(
        self._CreatePermissionsDataSource, PermissionsDataSource)

  def SetTemplateDataSource(self, template_data_source_factory):
    '''Initialize a template data source to be used to render partial templates
    into descriptions for permissions. Must be called before .get
    '''
    self._template_data_source = template_data_source_factory.Create(None, '')

  def _CreatePermissionsDataSource(self, _, content):
    '''Combine the contents of |_permissions_json_path| and
    |_permissions_features_path|. Filter into lists, one for extensions and
    one for apps.
    '''
    api_features = Parse(self._file_system.ReadSingle(self._api_features_path))

    def filter_for_platform(permissions, platform):
      return _ListifyPermissions(features.Filtered(permissions, platform))

    permissions_json = Parse(self._file_system.ReadSingle(
        self._permissions_json_path))
    permission_features = features.MergedWith(
        features.Parse(Parse(content)), permissions_json)

    _AddDependencyDescriptions(permission_features, api_features)
    # Turn partial templates into descriptions, ensure anchors are set.
    for permission in permission_features.values():
      if not 'anchor' in permission:
        permission['anchor'] = permission['name']
      if 'partial' in permission:
        permission['description'] = self._template_data_source.get(
            permission['partial'])
        del permission['partial']

    return {
      'declare_apps': filter_for_platform(permission_features, 'app'),
      'declare_extensions': filter_for_platform(
          permission_features, 'extension')
    }

  def get(self, key):
    return self._cache.GetFromFile(self._permissions_features_path)[key]
