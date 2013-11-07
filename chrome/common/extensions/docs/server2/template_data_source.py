# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import posixpath
import traceback

from data_source import DataSource
from docs_server_utils import FormatKey
from file_system import FileNotFoundError
from future import Future
from svn_constants import PRIVATE_TEMPLATE_PATH


class TemplateDataSource(DataSource):
  '''Provides a DataSource interface for compiled templates.
  '''

  def __init__(self, server_instance, _, partial_dir=PRIVATE_TEMPLATE_PATH):
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
