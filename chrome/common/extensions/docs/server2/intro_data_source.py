# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from HTMLParser import HTMLParser
import logging
import os
import re

from data_source import DataSource
from docs_server_utils import FormatKey
from document_parser import ParseDocument, RemoveTitle
from extensions_paths import INTROS_TEMPLATES, ARTICLES_TEMPLATES
from file_system import FileNotFoundError
from future import Future
from third_party.handlebar import Handlebar


class _HandlebarWithContext(Handlebar):
  '''Extends Handlebar with a get() method so that templates can not only
  render intros with {{+intro}} but also access properties on them, like
  {{intro.title}}, {{intro.toc}}, etc.
  '''

  def __init__(self, text, name, context):
    Handlebar.__init__(self, text, name=name)
    self._context = context

  def get(self, key):
    return self._context.get(key)


# TODO(kalman): rename this HTMLDataSource or other, then have separate intro
# article data sources created as instances of it.
class IntroDataSource(DataSource):
  '''This class fetches the intros for a given API. From this intro, a table
  of contents dictionary is created, which contains the headings in the intro.
  '''

  def __init__(self, server_instance, request):
    self._template_renderer = server_instance.template_renderer
    self._request = request
    self._cache = server_instance.compiled_fs_factory.Create(
        server_instance.host_file_system_provider.GetTrunk(),
        self._MakeIntro,
        IntroDataSource)
    self._ref_resolver = server_instance.ref_resolver_factory.Create()

  def _MakeIntro(self, intro_path, intro):
    # Guess the name of the API from the path to the intro.
    api_name = os.path.splitext(intro_path.split('/')[-1])[0]
    intro_with_links = self._ref_resolver.ResolveAllLinks(
            intro, namespace=api_name)

    # The templates generate a title for intros based on the API name.
    expect_title = '/intros/' not in intro_path

    # TODO(kalman): In order to pick up every header tag, and therefore make a
    # complete TOC, the render context of the Handlebar needs to be passed
    # through to here. Even if there were a mechanism to do this it would
    # break caching; we'd need to do the TOC parsing *after* rendering the full
    # template, and that would probably be expensive.
    parse_result = ParseDocument(
        self._template_renderer.Render(Handlebar(intro_with_links),
                                       self._request,
                                       # Avoid nasty surprises.
                                       data_sources=('partials', 'strings'),
                                       warn=False),
        expect_title=expect_title)

    if parse_result.warnings:
      logging.warning('%s: %s' % (intro_path, '\n'.join(parse_result.warnings)))

    # Convert the TableOfContentEntries into the data the templates want.
    def make_toc(entries):
      return [{
        'link': entry.attributes.get('id', ''),
        'subheadings': make_toc(entry.entries),
        'title': entry.name,
      } for entry in entries]

    if expect_title:
      intro_with_links, warning = RemoveTitle(intro_with_links)
      if warning:
        logging.warning('%s: %s' % (intro_path, warning))

    if parse_result.sections:
      # Only use the first section for now.
      toc = make_toc(parse_result.sections[0].structure)
    else:
      toc = []

    return _HandlebarWithContext(intro_with_links, intro_path, {
      'title': parse_result.title,
      'toc': toc
    })

  def get(self, key):
    path = FormatKey(key)
    def get_from_base_path(base_path):
      return self._cache.GetFromFile('%s/%s' % (base_path, path)).Get()
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
