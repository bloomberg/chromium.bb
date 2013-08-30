# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging

from data_source import DataSource
from third_party.json_schema_compiler.json_parse import Parse


def _AddLevels(items, level):
  '''Add a 'level' key to each item in |items|. 'level' corresponds to how deep
  in |items| an item is. |level| sets the starting depth.
  '''
  for item in items:
    item['level'] = level
    if 'items' in item:
      _AddLevels(item['items'], level + 1)


def _AddSelected(items, path):
  '''Add 'selected' and 'child_selected' properties to |items| so that the
  sidenav can be expanded to show which menu item has been selected. Returns
  True if an item was marked 'selected'.
  '''
  for item in items:
    if item.get('href', '') == '/' + path:
      item['selected'] = True
      return True
    if 'items' in item:
      if _AddSelected(item['items'], path):
        item['child_selected'] = True
        return True

  return False


def _QualifyHrefs(items):
  '''Force hrefs in |items| to either be absolute (http://...) or qualified
  (begins with a slash (/)). Other hrefs emit a warning and should be updated.
  '''
  for item in items:
    if 'items' in item:
      _QualifyHrefs(item['items'])

    href = item.get('href')
    if href is not None and not href.startswith(('http://', 'https://')):
      if not href.startswith('/'):
        logging.warn('Paths in sidenav must be qualified. %s is not.' % href)
        href = '/' + href
      item['href'] = href


def _CreateSidenavDict(_, content):
  items = Parse(content)
  # Start at level 2, the top <ul> element is level 1.
  _AddLevels(items, level=2)
  _QualifyHrefs(items)
  return items


class SidenavDataSource(DataSource):
  '''Provides templates with access to JSON files used to create the side
  navigation bar.
  '''
  def __init__(self, server_instance, request):
    self._cache = server_instance.compiled_host_fs_factory.Create(
        _CreateSidenavDict, SidenavDataSource)
    self._json_path = server_instance.sidenav_json_base_path
    self._request = request

  def Cron(self):
    for platform in ['apps', 'extensions']:
      self._cache.GetFromFile(
          '%s/%s_sidenav.json' % (self._json_path, platform))

  def get(self, key):
    sidenav = copy.deepcopy(self._cache.GetFromFile(
        '%s/%s_sidenav.json' % (self._json_path, key)))
    _AddSelected(sidenav, self._request.path)
    return sidenav
