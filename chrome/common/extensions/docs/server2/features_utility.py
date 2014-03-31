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

from copy import copy

from branch_utility import BranchUtility


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
    '''Returns true if this feature should be ignored. Features are ignored if
    they are only available to whitelisted apps or component extensions/apps, as
    in these cases the APIs are not available to public developers.

    Private APIs are also unavailable to public developers, but logic elsewhere
    makes sure they are not listed. So they shouldn't be ignored via this
    mechanism.
    '''
    if name.endswith('Private'):
      return False
    return value.get('location') == 'component' or 'whitelist' in value

  for name, rawvalue in features_json.iteritems():
    # Some feature names correspond to a list, typically because they're
    # whitelisted in stable for certain extensions and available in dev for
    # everybody else. Force a list down to a single feature by attempting to
    # remove the entries that don't affect the typical usage of an API.
    if isinstance(rawvalue, list):
      available_values = [subvalue for subvalue in rawvalue
                          if not ignore_feature(name, subvalue)]
      if not available_values:
        continue

      if len(available_values) == 1:
        value = available_values[0]
      else:
        # Multiple available values probably implies different feature
        # configurations for apps vs extensions. Currently, this is 'commands'.
        # To get the ball rolling, add a hack to combine the extension types.
        # See http://crbug.com/316194.
        extension_types = set()
        for available_value in available_values:
          extension_types.update(available_value.get('extension_types', ()))

        # For the base value, select the one with the most recent availability.
        channel_names = BranchUtility.GetAllChannelNames()
        available_values.sort(
            key=lambda v: channel_names.index(v.get('channel', 'stable')))

        # Note: copy() is enough because only a single toplevel key is modified.
        value = copy(available_values[0])
        value['extension_types'] = list(extension_types)
    else:
      value = rawvalue

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


def Filtered(features, platform):
  '''Create a new Features dictionary from |features| that contains only items
  relevant to |platform|. Feature values are unmodified; callers should
  deepcopy() if they need to modify them.
  '''
  return dict((name, feature) for name, feature in features.iteritems()
              if platform in feature['platforms'])


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
