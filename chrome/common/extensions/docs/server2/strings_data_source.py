# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from data_source import DataSource
from third_party.json_schema_compiler.json_parse import Parse

class StringsDataSource(DataSource):
  '''Provides templates with access to a key to string mapping defined in a
  JSON configuration file.
  '''
  def __init__(self, server_instance, _):
    self._cache = server_instance.compiled_host_fs_factory.Create(
        lambda _, strings_json: Parse(strings_json), StringsDataSource)
    self._strings_json_path = server_instance.strings_json_path

  def Cron(self):
    self._cache.GetFromFile(self._strings_json_path)

  def get(self, key):
    return self._cache.GetFromFile(self._strings_json_path)[key]
