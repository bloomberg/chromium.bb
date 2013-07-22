# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import json
import logging

class SidenavDataSource(object):
  """This class reads in and caches a JSON file representing the side navigation
  menu.
  """
  class Factory(object):
    def __init__(self, compiled_fs_factory, json_path):
      self._cache = compiled_fs_factory.Create(self._CreateSidenavDict,
                                               SidenavDataSource)
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
    self._href = '/' + path

  def _AddSelected(self, items):
    for item in items:
      if item.get('href', '') == self._href:
        item['selected'] = True
        return True
      if 'items' in item:
        if self._AddSelected(item['items']):
          item['child_selected'] = True
          return True
    return False

  def _QualifyHrefs(self, items):
    for item in items:
      if 'items' in item:
        self._QualifyHrefs(item['items'])

      href = item.get('href')
      if href is not None and not href.startswith(('http://', 'https://')):
        if not href.startswith('/'):
          logging.warn('Paths in sidenav must be qualified. %s is not.' % href)
          href = '/' + href
        item['href'] = href

  def get(self, key):
    sidenav = copy.deepcopy(self._cache.GetFromFile(
        '%s/%s_sidenav.json' % (self._json_path, key)))
    self._AddSelected(sidenav)
    self._QualifyHrefs(sidenav)
    return sidenav
