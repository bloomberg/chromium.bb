# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from path_utils import FormatKey
from third_party.handlebar import Handlebar

EXTENSIONS_URL = '/chrome/extensions'

def _MakeBranchDict(branch):
  return {
    'showWarning': branch != 'stable',
    'branches': [
      { 'name': 'Stable', 'path': EXTENSIONS_URL + '/stable' },
      { 'name': 'Dev',    'path': EXTENSIONS_URL + '/dev' },
      { 'name': 'Beta',   'path': EXTENSIONS_URL + '/beta' },
      { 'name': 'Trunk',  'path': EXTENSIONS_URL + '/trunk' }
    ],
    'current': branch
  }

class TemplateDataSource(object):
  """ Renders Handlebar templates, providing them with the context in which to
  render.

  Also acts as a data source itself, providing partial Handlebar templates to
  those it renders.

  Each instance of TemplateDataSource is bound to a Request so that it can
  render templates with request-specific data (such as Accept-Language); use
  a Factory to cheaply construct these.
  """

  class Factory(object):
    """ A factory to create lightweight TemplateDataSource instances bound to
    individual Requests.
    """
    def __init__(self,
                 branch,
                 api_data_source,
                 intro_data_source,
                 samples_data_source,
                 cache_builder,
                 base_paths):
      self._branch_info = _MakeBranchDict(branch)
      self._static_resources = ((('/' + branch) if branch != 'local' else '') +
                                '/static')
      self._api_data_source = api_data_source
      self._intro_data_source = intro_data_source
      self._samples_data_source = samples_data_source
      self._cache = cache_builder.build(Handlebar)
      self._base_paths = base_paths

    def Create(self, request):
      """ Returns a new TemplateDataSource bound to |request|.
      """
      return TemplateDataSource(self._branch_info,
                                self._static_resources,
                                self._api_data_source,
                                self._intro_data_source,
                                self._samples_data_source,
                                self._cache,
                                self._base_paths,
                                request)

  def __init__(self,
               branch_info,
               static_resources,
               api_data_source,
               intro_data_source,
               samples_data_source,
               cache,
               base_paths,
               request):
    self._branch_info = branch_info
    self._static_resources = static_resources
    self._api_data_source = api_data_source
    self._intro_data_source = intro_data_source
    self._samples_data_source = samples_data_source
    self._cache = cache
    self._base_paths = base_paths
    self._request = request

  def Render(self, template_name):
    """This method will render a template named |template_name|, fetching all
    the partial templates needed from |self._cache|. Partials are retrieved
    from the TemplateDataSource with the |get| method.
    """
    template = self.get(template_name)
    if not template:
      return ''
      # TODO error handling
    return template.render({
      'apis': self._api_data_source,
      'branchInfo': self._branch_info,
      'intros': self._intro_data_source,
      'partials': self,
      'samples': self._samples_data_source,
      'static': self._static_resources
    }).text

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    real_path = FormatKey(key)
    for base_path in self._base_paths:
      try:
        return self._cache.GetFromFile(base_path + '/' + real_path)
      except Exception:
        pass
    return None
