# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from HTMLParser import HTMLParser
import logging
import os
import re

from docs_server_utils import FormatKey
from file_system import FileNotFoundError
import compiled_file_system as compiled_fs
from third_party.handlebar import Handlebar

# Increment this version if there are changes to the table of contents dict that
# IntroDataSource caches.
_VERSION = 4

_H1_REGEX = re.compile('<h1[^>.]*?>.*?</h1>', flags=re.DOTALL)

class _IntroParser(HTMLParser):
  """ An HTML parser which will parse table of contents and page title info out
  of an intro.
  """
  def __init__(self):
    HTMLParser.__init__(self)
    self.toc = []
    self.page_title = None
    self._recent_tag = None
    self._current_heading = {}

  def handle_starttag(self, tag, attrs):
    id_ = ''
    if tag not in ['h1', 'h2', 'h3']:
      return
    if tag != 'h1' or self.page_title is None:
      self._recent_tag = tag
    for attr in attrs:
      if attr[0] == 'id':
        id_ = attr[1]
    if tag == 'h2':
      self._current_heading = { 'link': id_, 'subheadings': [], 'title': '' }
      self.toc.append(self._current_heading)
    elif tag == 'h3':
      self._current_heading = { 'link': id_, 'title': '' }
      self.toc[-1]['subheadings'].append(self._current_heading)

  def handle_endtag(self, tag):
    if tag in ['h1', 'h2', 'h3']:
      self._recent_tag = None

  def handle_data(self, data):
    if self._recent_tag is None:
      return
    if self._recent_tag == 'h1':
      if self.page_title is None:
        self.page_title = data
      else:
        self.page_title += data
    elif self._recent_tag in ['h2', 'h3']:
      self._current_heading['title'] += data

class IntroDataSource(object):
  """This class fetches the intros for a given API. From this intro, a table
  of contents dictionary is created, which contains the headings in the intro.
  """
  class Factory(object):
    def __init__(self, cache_factory, ref_resolver_factory, base_paths):
      self._cache = cache_factory.Create(self._MakeIntroDict,
                                         compiled_fs.INTRO,
                                         version=_VERSION)
      self._ref_resolver = ref_resolver_factory.Create()
      self._base_paths = base_paths

    def _MakeIntroDict(self, intro_path, intro):
      # Guess the name of the API from the path to the intro.
      api_name = os.path.splitext(intro_path.split('/')[-1])[0]
      intro_with_links = self._ref_resolver.ResolveAllLinks(intro,
                                                            namespace=api_name)
      apps_parser = _IntroParser()
      apps_parser.feed(Handlebar(intro_with_links).render(
          { 'is_apps': True }).text)
      extensions_parser = _IntroParser()
      extensions_parser.feed(Handlebar(intro_with_links).render(
          { 'is_apps': False }).text)
      # TODO(cduvall): Use the normal template rendering system, so we can check
      # errors.
      if extensions_parser.page_title != apps_parser.page_title:
        logging.error(
            'Title differs for apps and extensions: Apps: %s, Extensions: %s.' %
                (extensions_parser.page_title, apps_parser.page_title))
      # The templates will render the heading themselves, so remove it from the
      # HTML content.
      intro_with_links = re.sub(_H1_REGEX, '', intro_with_links, count=1)
      return {
        'intro': Handlebar(intro_with_links),
        'title': apps_parser.page_title,
        'apps_toc': apps_parser.toc,
        'extensions_toc': extensions_parser.toc,
      }

    def Create(self):
      return IntroDataSource(self._cache, self._base_paths)

  def __init__(self, cache, base_paths):
    self._cache = cache
    self._base_paths = base_paths

  def get(self, key):
    real_path = FormatKey(key)
    error = None
    for base_path in self._base_paths:
      try:
        return self._cache.GetFromFile(base_path + '/' + real_path)
      except FileNotFoundError as error:
        pass
    raise ValueError(str(error) + ': No intro found for "%s".' % key)
