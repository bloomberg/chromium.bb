# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import collections
import os

import svn_constants
from branch_utility import BranchUtility
from compiled_file_system import CompiledFileSystem
from file_system import FileNotFoundError
from third_party.json_schema_compiler import json_parse
from third_party.json_schema_compiler.memoize import memoize
from third_party.json_schema_compiler.model import UnixName


def _GetChannelFromFeatures(api_name, file_system, path):
  '''Finds API channel information within _features.json files at the given
  |path| for the given |file_system|. Returns None if channel information for
  the API cannot be located.
  '''
  feature = file_system.GetFromFile(path).get(api_name)

  if feature is None:
    return None
  if isinstance(feature, collections.Mapping):
    # The channel information exists as a solitary dict.
    return feature.get('channel')
  # The channel information dict is nested within a list for whitelisting
  # purposes. Take the newest channel out of all of the entries.
  return BranchUtility.NewestChannel(entry.get('channel') for entry in feature)


def _GetChannelFromApiFeatures(api_name, file_system):
  return _GetChannelFromFeatures(
      api_name,
      file_system,
      '%s/_api_features.json' % svn_constants.API_PATH)


def _GetChannelFromManifestFeatures(api_name, file_system):
  return _GetChannelFromFeatures(
      UnixName(api_name), #_manifest_features uses unix_style API names
      file_system,
      '%s/_manifest_features.json' % svn_constants.API_PATH)


def _GetChannelFromPermissionFeatures(api_name, file_system):
  return _GetChannelFromFeatures(
      api_name,
      file_system,
      '%s/_permission_features.json' % svn_constants.API_PATH)


def _ExistsInExtensionApi(api_name, file_system):
  '''Parses the api/extension_api.json file (available in Chrome versions
  before 18) for an API namespace. If this is successfully found, then the API
  is considered to have been 'stable' for the given version.
  '''
  try:
    extension_api_json = file_system.GetFromFile(
        '%s/extension_api.json' % svn_constants.API_PATH)
    api_rows = [row.get('namespace') for row in extension_api_json
                if 'namespace' in row]
    return api_name in api_rows
  except FileNotFoundError:
    # This should only happen on preview.py since extension_api.json is no
    # longer present in trunk.
    return False


class AvailabilityFinder(object):
  '''Generates availability information for APIs by looking at API schemas and
  _features files over multiple release versions of Chrome.
  '''

  def __init__(self,
               file_system_iterator,
               object_store_creator,
               branch_utility):
    self._file_system_iterator = file_system_iterator
    self._object_store_creator = object_store_creator
    self._object_store = self._object_store_creator.Create(AvailabilityFinder)
    self._branch_utility = branch_utility

  def _ExistsInFileSystem(self, api_name, file_system):
    '''Checks for existence of |api_name| within the list of api files in the
    api/ directory found using the given |file_system|.
    '''
    file_names = file_system.ReadSingle('%s/' % svn_constants.API_PATH)
    api_names = tuple(os.path.splitext(name)[0] for name in file_names
                      if os.path.splitext(name)[1][1:] in ['json', 'idl'])

    # API file names in api/ are unix_name at every version except for versions
    # 18, 19, and 20. Since unix_name is the more common format, check it first.
    return (UnixName(api_name) in api_names) or (api_name in api_names)

  def _CheckStableAvailability(self, api_name, file_system, version):
    '''Checks for availability of an API, |api_name|, on the stable channel.
    Considers several _features.json files, file system existence, and
    extension_api.json depending on the given |version|.
    '''
    if version < 5:
      # SVN data isn't available below version 5.
      return False
    available_channel = None
    fs_factory = CompiledFileSystem.Factory(file_system,
                                            self._object_store_creator)
    features_fs = fs_factory.Create(lambda _, json: json_parse.Parse(json),
                                    AvailabilityFinder,
                                    category='features')
    if version >= 28:
      # The _api_features.json file first appears in version 28 and should be
      # the most reliable for finding API availability.
      available_channel = _GetChannelFromApiFeatures(api_name, features_fs)
    if version >= 20:
      # The _permission_features.json and _manifest_features.json files are
      # present in Chrome 20 and onwards. Use these if no information could be
      # found using _api_features.json.
      available_channel = available_channel or (
          _GetChannelFromPermissionFeatures(api_name, features_fs)
          or _GetChannelFromManifestFeatures(api_name, features_fs))
      if available_channel is not None:
        return available_channel == 'stable'
    if version >= 18:
      # Fall back to a check for file system existence if the API is not
      # stable in any of the _features.json files, OR if we're dealing with
      # version 18 or 19, which don't contain relevant _features information.
      return self._ExistsInFileSystem(api_name, file_system)
    if version >= 5:
      # Versions 17 down to 5 have an extension_api.json file which
      # contains namespaces for each API that was available at the time.
      return _ExistsInExtensionApi(api_name, features_fs)

  def _CheckChannelAvailability(self, api_name, file_system, channel_name):
    '''Searches through the _features files in a given |file_system| and
    determines whether or not an API is available on the given channel,
    |channel_name|.
    '''
    fs_factory = CompiledFileSystem.Factory(file_system,
                                            self._object_store_creator)
    features_fs = fs_factory.Create(lambda _, json: json_parse.Parse(json),
                                    AvailabilityFinder,
                                    category='features')
    available_channel = (_GetChannelFromApiFeatures(api_name, features_fs)
        or _GetChannelFromPermissionFeatures(api_name, features_fs)
        or _GetChannelFromManifestFeatures(api_name, features_fs))
    if (available_channel is None and
        self._ExistsInFileSystem(api_name, file_system)):
      # If an API is not represented in any of the _features files, but exists
      # in the filesystem, then assume it is available in this version.
      # The windows API is an example of this.
      available_channel = channel_name
    # If the channel we're checking is the same as or newer than the
    # |available_channel| then the API is available at this channel.
    return (available_channel is not None and
            BranchUtility.NewestChannel((available_channel, channel_name))
                == channel_name)

  def _CheckApiAvailability(self, api_name, file_system, channel_info):
    '''Determines the availability for an API at a certain version of Chrome.
    Two branches of logic are used depending on whether or not the API is
    determined to be 'stable' at the given version.
    '''
    if channel_info.channel == 'stable':
      return self._CheckStableAvailability(api_name,
                                           file_system,
                                           channel_info.version)
    return self._CheckChannelAvailability(api_name,
                                          file_system,
                                          channel_info.channel)

  def GetApiAvailability(self, api_name):
    '''Performs a search for an API's top-level availability by using a
    HostFileSystemIterator instance to traverse multiple version of the
    SVN filesystem.
    '''
    availability = self._object_store.Get(api_name).Get()
    if availability is not None:
      return availability

    def check_api_availability(file_system, channel_info):
      return self._CheckApiAvailability(api_name, file_system, channel_info)

    availability = self._file_system_iterator.Descending(
        self._branch_utility.GetChannelInfo('dev'),
        check_api_availability)
    if availability is None:
      # The API wasn't available on 'dev', so it must be a 'trunk'-only API.
      availability = self._branch_utility.GetChannelInfo('trunk')
    self._object_store.Set(api_name, availability)
    return availability
