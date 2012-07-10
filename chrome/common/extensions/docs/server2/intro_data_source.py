# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
from path_utils import FormatKey
from third_party.handlebar import Handlebar

class IntroDataSource(object):
  """This class fetches and loads JSON APIs with the fetcher passed in with
  |cache_builder|, so the APIs can be plugged into templates.
  """
  def __init__(self, cache_builder, base_path):
    self._cache = cache_builder.build(self._MakeIntroDict)
    self._base_path = base_path

  def _MakeIntroDict(self, intro):
    headings = re.findall('<h([23]) id\="(.+)">(.+)</h[23]>', intro)
    toc = []
    for heading in headings:
      level, link, title = heading
      if level == '2':
        toc.append({ 'link': link, 'title': title, 'subheadings': [] })
      else:
        toc[-1]['subheadings'].append({ 'link': link, 'title': title })
    return { 'intro': Handlebar(intro), 'toc': toc }

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    real_path = FormatKey(key)
    try:
      return self._cache.get(self._base_path + '/' + real_path)
    except:
      pass
    return None
