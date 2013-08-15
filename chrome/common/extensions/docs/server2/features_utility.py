# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from copy import deepcopy

def Parse(features_json):
  '''Standardize the raw features file json |features_json| into a more regular
  format. Return a dictionary of features, keyed by name. Each feature is
  guaranteed to have a 'name' attribute and a 'platforms' attribute that is a
  list of platforms the feature is relevant to. Valid platforms are 'app' or
  'extension'. Other optional keys may be present in each feature.

  Features with a 'whitelist' are only relevant to apps or extensions on that
  whitelist and are not included in the retured features dictionary.
  '''
  features = {}

  for name, value in features_json.iteritems():
    # Some feature names corrispond to a list; force a list down to a single
    # feature by removing entries that have a 'whitelist'.
    if isinstance(value, list):
      values = [subvalue for subvalue in value if not 'whitelist' in subvalue]
      if values:
        value = values[0]
      else:
        continue

    if 'whitelist' in value:
      continue

    features[name] = { 'platforms': [] }

    platforms = value.pop('extension_types')
    if platforms == 'all' or 'platform_app' in platforms:
      features[name]['platforms'].append('app')
    if platforms == 'all' or 'extension' in platforms:
      features[name]['platforms'].append('extension')

    features[name]['name'] = name
    features[name].update(value)

  return features

def Filtered(features, platform=None):
  '''Create a new features dictionary from |features| that contains only items
  relevant to |platform|. Items retained are deepcopied. Returns new features
  dictionary.
  '''
  filtered_features = {}

  for name, feature in features.iteritems():
    if not platform or platform in feature['platforms']:
      filtered_features[name] = deepcopy(feature)

  return filtered_features

def MergedWith(features, other):
  '''Merge |features| with an additional dictionary to create a new features
  dictionary. If a feature is common to both |features| and |other|, then it is
  merged using the standard dictionary update instead of being overwritten.
  Returns the new features dictionary.
  '''
  for key, value in other.iteritems():
    if key in features:
      features[key].update(value)
    else:
      features[key] = value

  return features
