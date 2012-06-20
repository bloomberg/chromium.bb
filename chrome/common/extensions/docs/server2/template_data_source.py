# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from third_party.handlebar import Handlebar

class TemplateDataSource(object):
  """This class fetches and compiles templates using the fetcher passed in with
  |cache_builder|.
  """
  def __init__(self, cache_builder, base_paths):
    self._cache = cache_builder.build(self._LoadTemplate)
    self._base_paths = base_paths

  def _LoadTemplate(self, template):
    return Handlebar(template)

  def Render(self, template_name, context):
    """This method will render a template named |template_name|, fetching all
    the partial templates needed from |self._cache|. Partials are retrieved
    from the TemplateDataSource with the |get| method.
    """
    template = self.get(template_name)
    if not template:
      return ''
      # TODO error handling
    return template.render(context, {'partials': self}).text

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    index = key.rfind('.html')
    if index > 0:
      key = key[:index]
    real_path = key + '.html'
    for base_path in self._base_paths:
      try:
        return self._cache.get(base_path + '/' + real_path)
      except:
        pass
    return None
