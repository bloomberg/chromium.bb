# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Utility functions for working with the Feature abstraction. Features are grouped
into a dictionary by name. Each Feature is guaranteed to have the following two
keys:
  name - a string, the name of the feature
  platform - a list containing 'app' or 'extension', both, or neither.

A Feature may have other keys from a _features.json file as well. Features with
a whitelist are ignored as they are only useful to specific apps or extensions.
'''

from copy import deepcopy

def Parse(features_json):
  '''Process JSON from a _features.json file, standardizing it into a dictionary
  of Features.
  '''
  features = {}

  for name, value in deepcopy(features_json).iteritems():
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
  '''Create a new Features dictionary from |features| that contains only items
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
  Returns the new Features dictionary.
  '''
  for key, value in other.iteritems():
    if key in features:
      features[key].update(value)
    else:
      features[key] = value

    # Ensure the Feature schema is enforced for all added items.
    if not 'name' in features[key]:
      features[key]['name'] = key
    if not 'platforms' in features[key]:
      features[key]['platforms'] = []

  return features
