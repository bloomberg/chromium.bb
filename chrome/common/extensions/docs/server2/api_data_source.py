# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os

import third_party.json_schema_compiler.json_comment_eater as json_comment_eater
import third_party.json_schema_compiler.model as model

class APIDataSource(object):
  """This class fetches and loads JSON APIs with the fetcher passed in with
  |cache_builder|, so the APIs can be plugged into templates.
  """
  def __init__(self, cache_builder, base_paths):
    self._cache = cache_builder.build(self._LoadAPI)
    self._base_paths = base_paths

  def _LoadAPI(self, api):
    return json.loads(json_comment_eater.Nom(api))[0]

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    path, ext = os.path.splitext(key)
    unix_name = model.UnixName(path)
    json_path = unix_name + '.json'
    for base_path in self._base_paths:
      try:
        return self._cache.get(base_path + '/' + json_path)
      except:
        pass
    return None
