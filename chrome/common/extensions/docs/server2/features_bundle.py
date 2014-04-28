# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import posixpath

from compiled_file_system import SingleFile, Unicode
from extensions_paths import API_PATHS, JSON_TEMPLATES
import features_utility
from file_system import FileNotFoundError
from future import Future
from third_party.json_schema_compiler.json_parse import Parse


_API_FEATURES = '_api_features.json'
_MANIFEST_FEATURES = '_manifest_features.json'
_PERMISSION_FEATURES = '_permission_features.json'


def _GetFeaturePaths(feature_file, *extra_paths):
  paths = [posixpath.join(api_path, feature_file) for api_path in API_PATHS]
  paths.extend(extra_paths)
  return paths


def _AddPlatformsFromDependencies(feature,
                                  api_features,
                                  manifest_features,
                                  permission_features):
  features_map = {
    'api': api_features,
    'manifest': manifest_features,
    'permission': permission_features,
  }
  dependencies = feature.get('dependencies')
  if dependencies is None:
    return ['apps', 'extensions']
  platforms = set()
  for dependency in dependencies:
    dep_type, dep_name = dependency.split(':')
    dependency_features = features_map[dep_type]
    dependency_feature = dependency_features.get(dep_name)
    # If the dependency can't be resolved, it is inaccessible and therefore
    # so is this feature.
    if dependency_feature is None:
      return []
    platforms = platforms.union(dependency_feature['platforms'])
  feature['platforms'] = list(platforms)


class _FeaturesCache(object):
  def __init__(self, file_system, compiled_fs_factory, json_paths):
    populate = self._CreateCache
    if len(json_paths) == 1:
      populate = SingleFile(populate)

    self._cache = compiled_fs_factory.Create(file_system, populate, type(self))
    self._text_cache = compiled_fs_factory.ForUnicode(file_system)
    self._json_path = json_paths[0]
    self._extra_paths = json_paths[1:]

  @Unicode
  def _CreateCache(self, _, features_json):
    extra_path_futures = [self._text_cache.GetFromFile(path)
                          for path in self._extra_paths]
    features = features_utility.Parse(Parse(features_json))
    for path_future in extra_path_futures:
      try:
        extra_json = path_future.Get()
      except FileNotFoundError:
        # Not all file system configurations have the extra files.
        continue
      features = features_utility.MergedWith(
          features_utility.Parse(Parse(extra_json)), features)
    return features

  def GetFeatures(self):
    if self._json_path is None:
      return Future(value={})
    return self._cache.GetFromFile(self._json_path)


class FeaturesBundle(object):
  '''Provides access to properties of API, Manifest, and Permission features.
  '''
  def __init__(self, file_system, compiled_fs_factory, object_store_creator):
    self._api_cache = _FeaturesCache(
        file_system,
        compiled_fs_factory,
        _GetFeaturePaths(_API_FEATURES))
    self._manifest_cache = _FeaturesCache(
        file_system,
        compiled_fs_factory,
        _GetFeaturePaths(_MANIFEST_FEATURES,
                         posixpath.join(JSON_TEMPLATES, 'manifest.json')))
    self._permission_cache = _FeaturesCache(
        file_system,
        compiled_fs_factory,
        _GetFeaturePaths(_PERMISSION_FEATURES,
                         posixpath.join(JSON_TEMPLATES, 'permissions.json')))
    # Namespace the object store by the file system ID because this class is
    # used by the availability finder cross-channel.
    # TODO(kalman): Configure this at the ObjectStore level.
    self._object_store = object_store_creator.Create(
        _FeaturesCache, category=file_system.GetIdentity())

  def GetPermissionFeatures(self):
    return self._permission_cache.GetFeatures()

  def GetManifestFeatures(self):
    return self._manifest_cache.GetFeatures()

  def GetAPIFeatures(self):
    api_features = self._object_store.Get('api_features').Get()
    if api_features is not None:
      return Future(value=api_features)

    api_features_future = self._api_cache.GetFeatures()
    manifest_features_future = self._manifest_cache.GetFeatures()
    permission_features_future = self._permission_cache.GetFeatures()
    def resolve():
      api_features = api_features_future.Get()
      manifest_features = manifest_features_future.Get()
      permission_features = permission_features_future.Get()
      # TODO(rockot): Handle inter-API dependencies more gracefully.
      # Not yet a problem because there is only one such case (windows -> tabs).
      # If we don't store this value before annotating platforms, inter-API
      # dependencies will lead to infinite recursion.
      for feature in api_features.itervalues():
        _AddPlatformsFromDependencies(
            feature, api_features, manifest_features, permission_features)
      self._object_store.Set('api_features', api_features)
      return api_features
    return Future(callback=resolve)
