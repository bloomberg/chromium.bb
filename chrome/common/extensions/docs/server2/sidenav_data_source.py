# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import copy
import logging

from compiled_file_system import SingleFile, Unicode
from data_source import DataSource
from extensions_paths import JSON_TEMPLATES
from future import Gettable, Future
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
    if item.get('href', '') == path:
      item['selected'] = True
      return True
    if 'items' in item:
      if _AddSelected(item['items'], path):
        item['child_selected'] = True
        return True

  return False


class SidenavDataSource(DataSource):
  '''Provides templates with access to JSON files used to create the side
  navigation bar.
  '''
  def __init__(self, server_instance, request):
    self._cache = server_instance.compiled_fs_factory.Create(
        server_instance.host_file_system_provider.GetTrunk(),
        self._CreateSidenavDict,
        SidenavDataSource)
    self._server_instance = server_instance
    self._request = request

  @SingleFile
  @Unicode
  def _CreateSidenavDict(self, _, content):
    items = Parse(content)
    # Start at level 2, the top <ul> element is level 1.
    _AddLevels(items, level=2)
    self._QualifyHrefs(items)
    return items

  def _QualifyHrefs(self, items):
    '''Force hrefs in |items| to either be absolute (http://...) or qualified
    (beginning with /, in which case it will be moved relative to |base_path|).
    Relative hrefs emit a warning and should be updated.
    '''
    for item in items:
      if 'items' in item:
        self._QualifyHrefs(item['items'])

      href = item.get('href')
      if href is not None and not href.startswith(('http://', 'https://')):
        if not href.startswith('/'):
          logging.warn('Paths in sidenav must be qualified. %s is not.' % href)
        else:
          href = href.lstrip('/')
        item['href'] = self._server_instance.base_path + href

  def Cron(self):
    futures = [self._cache.GetFromFile('%s/%s_sidenav.json' %
                                       (JSON_TEMPLATES, platform))
               for platform in ('apps', 'extensions')]
    return Future(delegate=Gettable(lambda: [f.Get() for f in futures]))

  def get(self, key):
    sidenav = copy.deepcopy(self._cache.GetFromFile(
        '%s/%s_sidenav.json' % (JSON_TEMPLATES, key)).Get())
    _AddSelected(sidenav, self._server_instance.base_path + self._request.path)
    return sidenav
