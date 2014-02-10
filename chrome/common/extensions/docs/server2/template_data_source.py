# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import posixpath
import traceback

from data_source import DataSource
from extensions_paths import PRIVATE_TEMPLATES
from file_system import FileNotFoundError
from future import Collect
from path_util import AssertIsDirectory


class TemplateDataSource(DataSource):
  '''Provides a DataSource interface for compiled templates.
  '''

  def __init__(self, server_instance, _, partial_dir=PRIVATE_TEMPLATES):
    AssertIsDirectory(partial_dir)
    self._template_cache = server_instance.compiled_fs_factory.ForTemplates(
        server_instance.host_file_system_provider.GetTrunk())
    self._partial_dir = partial_dir
    self._file_system = server_instance.host_file_system_provider.GetTrunk()

  def get(self, path):
    try:
      return self._template_cache.GetFromFile('%s%s.html' %
          (self._partial_dir, path)).Get()
    except FileNotFoundError:
      logging.warning(traceback.format_exc())
      return None

  def Cron(self):
    futures = []
    for root, _, files in self._file_system.Walk(self._partial_dir):
      futures += [self._template_cache.GetFromFile(
                      posixpath.join(self._partial_dir, root, f))
                  for f in files
                  if posixpath.splitext(f)[1] == '.html']
    return Collect(futures)
