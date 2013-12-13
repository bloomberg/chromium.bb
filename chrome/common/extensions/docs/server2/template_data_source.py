# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import traceback

from data_source import DataSource
from extensions_paths import PRIVATE_TEMPLATES
from file_system import FileNotFoundError
from future import Future


class TemplateDataSource(DataSource):
  '''Provides a DataSource interface for compiled templates.
  '''

  def __init__(self, server_instance, _, partial_dir=PRIVATE_TEMPLATES):
    self._template_cache = server_instance.compiled_fs_factory.ForTemplates(
        server_instance.host_file_system_provider.GetTrunk())
    self._partial_dir = partial_dir

  def get(self, path):
    try:
      return self._template_cache.GetFromFile('%s/%s.html' %
          (self._partial_dir, path)).Get()
    except FileNotFoundError:
      logging.warning(traceback.format_exc())
      return None

  def Cron(self):
    # TODO(kalman): Implement this; probably by finding all files that can be
    # compiled to templates underneath |self._partial_dir| and compiling them.
    return Future(value=())
