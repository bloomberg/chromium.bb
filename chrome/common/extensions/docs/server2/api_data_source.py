# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

from file_system import FileNotFoundError
from handlebar_dict_generator import HandlebarDictGenerator
import third_party.json_schema_compiler.json_comment_eater as json_comment_eater
import third_party.json_schema_compiler.model as model
import third_party.json_schema_compiler.idl_schema as idl_schema
import third_party.json_schema_compiler.idl_parser as idl_parser

class APIDataSource(object):
  """This class fetches and loads JSON APIs from the FileSystem passed in with
  |cache_builder|, so the APIs can be plugged into templates.
  """
  class Factory(object):
    def __init__(self, cache_builder, base_path, samples_factory):
      self._permissions_cache = cache_builder.build(self._LoadPermissions)
      self._json_cache = cache_builder.build(self._LoadJsonAPI)
      self._idl_cache = cache_builder.build(self._LoadIdlAPI)
      self._samples_factory = samples_factory
      self._base_path = base_path

    def Create(self, request):
      return APIDataSource(self._permissions_cache,
                           self._json_cache,
                           self._idl_cache,
                           self._base_path,
                           self._samples_factory.Create(request))

    def _LoadPermissions(self, json_str):
      return json.loads(json_comment_eater.Nom(json_str))

    def _LoadJsonAPI(self, api):
      return HandlebarDictGenerator(json.loads(json_comment_eater.Nom(api))[0])

    def _LoadIdlAPI(self, api):
      idl = idl_parser.IDLParser().ParseData(api)
      return HandlebarDictGenerator(idl_schema.IDLSchema(idl).process()[0])

  def __init__(self,
               permissions_cache,
               json_cache,
               idl_cache,
               base_path,
               samples):
    self._base_path = base_path
    self._permissions_cache = permissions_cache
    self._json_cache = json_cache
    self._idl_cache = idl_cache
    self._samples = samples

  def _GetFeature(self, path):
    # Remove 'experimental_' from path name to match the keys in
    # _permissions_features.json.
    path = path.replace('experimental_', '')
    try:
      perms = self._permissions_cache.GetFromFile(
          self._base_path + '/_permission_features.json')
    except FileNotFoundError:
      return None
    api_perms = perms.get(path, None)
    if api_perms is None:
      return None
    if api_perms['channel'] == 'dev':
      api_perms['dev'] = True
    return api_perms

  def _GenerateHandlebarContext(self, api_name, handlebar, path):
    return_dict = { 'permissions': self._GetFeature(path) }
    return_dict.update(handlebar.Generate(
        self._FilterSamples(api_name, self._samples.values())))
    return return_dict

  def _FilterSamples(self, api_name, samples):
    api_search = '.' + api_name + '.'
    return [sample for sample in samples
            if any(api_search in api['name'] for api in sample['api_calls'])]

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    path, ext = os.path.splitext(key)
    unix_name = model.UnixName(path)
    json_path = unix_name + '.json'
    idl_path = unix_name + '.idl'
    try:
      return self._GenerateHandlebarContext(key,
          self._json_cache.GetFromFile(self._base_path + '/' + json_path),
          path)
    except FileNotFoundError:
      try:
        return self._GenerateHandlebarContext(key,
            self._idl_cache.GetFromFile(self._base_path + '/' + idl_path),
            path)
      except FileNotFoundError as e:
        logging.error(e)
        raise
