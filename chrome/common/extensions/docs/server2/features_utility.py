# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

'''
Utility functions for working with the Feature abstraction. Features are grouped
into a dictionary by name. Each Feature is guaranteed to have the following two
keys:
  name - a string, the name of the feature
  platform - a list containing 'apps' or 'extensions', both, or neither.

A Feature may have other keys from a _features.json file as well. Features with
a whitelist are ignored as they are only useful to specific apps or extensions.
'''

from copy import deepcopy

def _GetPlatformsForExtensionTypes(extension_types):
  platforms = []
  if extension_types == 'all' or 'platform_app' in extension_types:
    platforms.append('apps')
  if extension_types == 'all' or 'extension' in extension_types:
    platforms.append('extensions')
  return platforms

def Parse(features_json):
  '''Process JSON from a _features.json file, standardizing it into a dictionary
  of Features.
  '''
  features = {}

  def ignore_feature(name, value):
    '''Returns true if this feature should be ignored. This is defined by the
    presence of a 'whitelist' property for non-private APIs. Private APIs
    shouldn't have whitelisted features ignored since they're inherently
    private. Logic elsewhere makes sure not to list private APIs.
    '''
    return 'whitelist' in value and not name.endswith('Private')

  for name, value in deepcopy(features_json).iteritems():
    # Some feature names correspond to a list, typically because they're
    # whitelisted in stable for certain extensions and available in dev for
    # everybody else. Force a list down to a single feature by attempting to
    # remove the entries that don't affect the typical usage of an API.
    if isinstance(value, list):
      available_values = [subvalue for subvalue in value
                          if not ignore_feature(name, subvalue)]
      if len(available_values) == 0:
        logging.warning('No available values for feature "%s"' % name)
        value = value[0]
      elif len(available_values) == 1:
        value = available_values[0]
      else:
        # Multiple available values probably implies different feature
        # configurations for apps vs extensions. Currently, this is 'commands'.
        # To get the ball rolling, add a hack to combine the extension types.
        # See http://crbug.com/316194.
        extension_types = set()
        for value in available_values:
          extension_types.update(value['extension_types'])
        value = [subvalue for subvalue in available_values
                 if subvalue['channel'] == 'stable'][0]
        value['extension_types'] = list(extension_types)

    if ignore_feature(name, value):
      continue

    # Now we transform 'extension_types' into the more useful 'platforms'.
    #
    # But first, note that 'platforms' has a double meaning. In the docserver
    # model (what we're in the process of generating) it means 'apps' vs
    # 'extensions'. In the JSON features as read from Chrome it means 'win' vs
    # 'mac'. Ignore the latter.
    value.pop('platforms', None)
    extension_types = value.pop('extension_types', None)

    platforms = []
    if extension_types is not None:
      platforms = _GetPlatformsForExtensionTypes(extension_types)

    features[name] = {
      'name': name,
      'platforms': platforms,
    }
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
