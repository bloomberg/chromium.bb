# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os

from handlebar_dict_generator import HandlebarDictGenerator
import third_party.json_schema_compiler.json_comment_eater as json_comment_eater
import third_party.json_schema_compiler.model as model
import third_party.json_schema_compiler.idl_schema as idl_schema
import third_party.json_schema_compiler.idl_parser as idl_parser

class APIDataSource(object):
  """This class fetches and loads JSON APIs with the fetcher passed in with
  |cache_builder|, so the APIs can be plugged into templates.
  """
  def __init__(self, cache_builder, base_path):
    self._json_cache = cache_builder.build(self._LoadJsonAPI)
    self._idl_cache = cache_builder.build(self._LoadIdlAPI)
    self._base_path = base_path

  def _LoadJsonAPI(self, api):
    generator = HandlebarDictGenerator(
        json.loads(json_comment_eater.Nom(api))[0])
    return generator.Generate()

  def _LoadIdlAPI(self, api):
    idl = idl_parser.IDLParser().ParseData(api)
    generator = HandlebarDictGenerator(idl_schema.IDLSchema(idl).process()[0])
    return generator.Generate()

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    path, ext = os.path.splitext(key)
    unix_name = model.UnixName(path)
    json_path = unix_name + '.json'
    idl_path = unix_name + '.idl'
    try:
      return self._json_cache.get(self._base_path + '/' + json_path)
    except:
      try:
        return self._idl_cache.get(self._base_path + '/' + idl_path)
      except:
        pass
    return None
