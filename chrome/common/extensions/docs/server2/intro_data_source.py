# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from compiled_file_system import Unicode
from data_source import DataSource
from docs_server_utils import FormatKey
from extensions_paths import INTROS_TEMPLATES, ARTICLES_TEMPLATES
from file_system import FileNotFoundError
from future import Future
from third_party.handlebar import Handlebar



# TODO(kalman): rename this HTMLDataSource or other, then have separate intro
# article data sources created as instances of it.
class IntroDataSource(DataSource):
  '''This class fetches the intros for a given API. From this intro, a table
  of contents dictionary is created, which contains the headings in the intro.
  '''

  def __init__(self, server_instance, request):
    self._request = request
    self._cache = server_instance.compiled_fs_factory.Create(
        server_instance.host_file_system_provider.GetTrunk(),
        self._MakeIntro,
        IntroDataSource)
    self._ref_resolver = server_instance.ref_resolver_factory.Create()

  @Unicode
  def _MakeIntro(self, intro_path, intro):
    # Guess the name of the API from the path to the intro.
    api_name = os.path.splitext(intro_path.split('/')[-1])[0]
    return Handlebar(
        self._ref_resolver.ResolveAllLinks(intro,
                                           relative_to=self._request.path,
                                           namespace=api_name),
        name=intro_path)

  def get(self, key):
    path = FormatKey(key)
    def get_from_base_path(base_path):
      return self._cache.GetFromFile(base_path + path).Get()
    base_paths = (INTROS_TEMPLATES, ARTICLES_TEMPLATES)
    for base_path in base_paths:
      try:
        return get_from_base_path(base_path)
      except FileNotFoundError:
        continue
    # Not found. Do the first operation again so that we get a stack trace - we
    # know that it'll fail.
    get_from_base_path(base_paths[0])
    raise AssertionError()

  def Cron(self):
    # TODO(kalman): Walk through the intros and articles directory.
    return Future(value=())
