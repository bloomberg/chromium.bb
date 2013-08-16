# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json

import features_utility as features
from third_party.json_schema_compiler.json_parse import Parse

def _ListifyAndSortDocs(features, app_name):
  '''Convert a |feautres| dictionary, and all 'children' dictionaries, into
  lists recursively. Sort lists first by 'level' then by name.
  '''
  def sort_key(item):
    '''Key function to sort items primarily by level (according to index into
    levels) then subsort by name.
    '''
    levels = ('required', 'recommended', 'only_one', 'optional')

    return (levels.index(item.get('level', 'optional')), item['name'])

  def coerce_example_to_feature(feature):
    '''To display json in examples more clearly, convert the example of
    |feature| into the feature format, with a name and children, to be rendered
    by the templates. Only applicable to examples that are dictionaries.
    '''
    if not isinstance(feature.get('example'), dict):
      if 'example' in feature:
        feature['example'] = json.dumps(feature['example'])
      return
    # Add any keys/value pairs in the dict as children
    for key, value in feature['example'].iteritems():
      if not 'children' in feature:
        feature['children'] = {}
      feature['children'][key] = { 'name': key, 'example': value }
    del feature['example']
    del feature['has_example']

  def convert_and_sort(features):
    for key, value in features.items():
      if 'example' in value:
        value['has_example'] = True
        example = json.dumps(value['example'])
        if example == '{}':
          value['example'] = '{...}'
        elif example == '[]':
          value['example'] = '[...]'
        else:
          coerce_example_to_feature(value)
      if 'children' in value:
        features[key]['children'] = convert_and_sort(value['children'])
    return sorted(features.values(), key=sort_key)

  # Replace {{title}} in the 'name' manifest property example with |app_name|
  if 'name' in features:
    name = features['name']
    name['example'] = name['example'].replace('{{title}}', app_name)

  features = convert_and_sort(features)

  return features

def _AddLevelAnnotations(features):
  '''Add level annotations to |features|. |features| and children lists must be
  sorted by 'level'. Annotations are added to the first item in a group of
  features of the same 'level'.

  The last item in a list has 'is_last' set to True.
  '''
  annotations = {
    'required': 'Required',
    'recommended': 'Recommended',
    'only_one': 'Pick one (or none)',
    'optional': 'Optional'
  }

  def add_annotation(item, annotation):
    if not 'annotations' in item:
      item['annotations'] = []
    item['annotations'].insert(0, annotation)

  def annotate(parent_level, features):
    current_level = parent_level
    for item in features:
      level = item.get('level', 'optional')
      if level != current_level:
        add_annotation(item, annotations[level])
        current_level = level
      if 'children' in item:
        annotate(level, item['children'])
    if features:
      features[-1]['is_last'] = True

  annotate('required', features)
  return features

def _RestructureChildren(features):
  '''Features whose names are of the form 'parent.child' are moved to be part
  of the 'parent' dictionary under the key 'children'. Names are changed to
  the 'child' section of the original name. Applied recursively so that
  children can have children.
  '''
  def add_child(features, parent, child_name, value):
    value['name'] = child_name
    if not 'children' in features[parent]:
      features[parent]['children'] = {}
    features[parent]['children'][child_name] = value

  def insert_children(features):
    for name in features.keys():
      if '.' in name:
        value = features.pop(name)
        parent, child_name = name.split('.', 1)
        add_child(features, parent, child_name, value)

    for value in features.values():
      if 'children' in value:
        insert_children(value['children'])

  insert_children(features)
  return features

class ManifestDataSource(object):
  '''Provides access to the properties in manifest features.
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

  def GetFeatures(self):
    '''Returns a dictionary of the contents of |_features_path| merged with
    |_manifest_path|.
    '''
    manifest_json = Parse(self._file_system.ReadSingle(self._manifest_path))
    manifest_features = features.Parse(
        Parse(self._file_system.ReadSingle(self._features_path)))

    return features.MergedWith(manifest_features, manifest_json)

  def _CreateManifestData(self, _, content):
    '''Combine the contents of |_manifest_path| and |_features_path| and filter
    the results into lists specific to apps or extensions for templates. Marks
    up features with annotations.
    '''
    def for_templates(manifest_features, platform):
      return _AddLevelAnnotations(
          _ListifyAndSortDocs(
              _RestructureChildren(
                  features.Filtered(manifest_features, platform)),
              app_name=platform.capitalize()))

    manifest_json = Parse(self._file_system.ReadSingle(self._manifest_path))
    manifest_features = features.MergedWith(
        features.Parse(Parse(content)), manifest_json)

    return {
      'apps': for_templates(manifest_features, 'app'),
      'extensions': for_templates(manifest_features, 'extension')
    }

  def get(self, key):
    return self._cache.GetFromFile(self._features_path)[key]
