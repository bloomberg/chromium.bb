# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import logging

import compiled_file_system as compiled_fs
from file_system import FileNotFoundError
from third_party.json_schema_compiler.model import UnixName

class SidenavDataSource(object):
  """This class reads in and caches a JSON file representing the side navigation
  menu.
  """
  class Factory(object):
    def __init__(self, cache_factory, json_path):
      self._cache = cache_factory.Create(self._CreateSidenavDict,
                                         compiled_fs.SIDENAV)
      self._json_path = json_path

    def Create(self, path):
      """Create a SidenavDataSource, binding it to |path|. |path| is the url
      of the page that is being rendered. It is used to determine which item
      in the sidenav should be highlighted.
      """
      return SidenavDataSource(self._cache, self._json_path, path)

    def _AddLevels(self, items, level):
      """Levels represent how deeply this item is nested in the sidenav. We
      start at 2 because the top <ul> is the only level 1 element.
      """
      for item in items:
        item['level'] = level
        if 'items' in item:
          self._AddLevels(item['items'], level + 1)

    def _CreateSidenavDict(self, json_path, json_str):
      items = json.loads(json_str)
      self._AddLevels(items, 2);
      return items

  def __init__(self, cache, json_path, path):
    self._cache = cache
    self._json_path = json_path
    self._file_name = path.split('/')[-1]

  def _AddSelected(self, items):
    for item in items:
      if item.get('fileName', '') == self._file_name:
        item['selected'] = True
        return True
      if 'items' in item:
        if self._AddSelected(item['items']):
          item['child_selected'] = True
    return False

  def get(self, key):
    try:
      sidenav = copy.deepcopy(self._cache.GetFromFile(
          '%s/%s_sidenav.json' % (self._json_path, key)))
      self._AddSelected(sidenav)
      return sidenav
    except FileNotFoundError as e:
      logging.error('%s: Error reading sidenav "%s".' % (e, key))
