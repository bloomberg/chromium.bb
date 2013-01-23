# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from docs_server_utils import FormatKey
import compiled_file_system as compiled_fs
from file_system import FileNotFoundError
from third_party.handlebar import Handlebar
import url_constants

# Increment this if there are changes to the data stored about templates.
_VERSION = 1

EXTENSIONS_URL = '/chrome/extensions'

def _MakeChannelDict(channel_name):
  return {
    'showWarning': channel_name != 'stable',
    'channels': [
      { 'name': 'Stable', 'path': 'stable' },
      { 'name': 'Dev',    'path': 'dev' },
      { 'name': 'Beta',   'path': 'beta' },
      { 'name': 'Trunk',  'path': 'trunk' }
    ],
    'current': channel_name
  }

class TemplateDataSource(object):
  """Renders Handlebar templates, providing them with the context in which to
  render.

  Also acts as a data source itself, providing partial Handlebar templates to
  those it renders.

  Each instance of TemplateDataSource is bound to a Request so that it can
  render templates with request-specific data (such as Accept-Language); use
  a Factory to cheaply construct these.
  """

  class Factory(object):
    """A factory to create lightweight TemplateDataSource instances bound to
    individual Requests.
    """
    def __init__(self,
                 channel_name,
                 api_data_source_factory,
                 api_list_data_source_factory,
                 intro_data_source_factory,
                 samples_data_source_factory,
                 known_issues_data_source,
                 sidenav_data_source_factory,
                 cache_factory,
                 ref_resolver_factory,
                 public_template_path,
                 private_template_path):
      self._branch_info = _MakeChannelDict(channel_name)
      self._api_data_source_factory = api_data_source_factory
      self._api_list_data_source_factory = api_list_data_source_factory
      self._intro_data_source_factory = intro_data_source_factory
      self._samples_data_source_factory = samples_data_source_factory
      self._known_issues_data_source = known_issues_data_source
      self._sidenav_data_source_factory = sidenav_data_source_factory
      self._cache = cache_factory.Create(self._CreateTemplate,
                                         compiled_fs.HANDLEBAR,
                                         version=_VERSION)
      self._ref_resolver = ref_resolver_factory.Create()
      self._public_template_path = public_template_path
      self._private_template_path = private_template_path
      self._static_resources = '/%s/static' % channel_name

    def _CreateTemplate(self, template_name, text):
      return Handlebar(self._ref_resolver.ResolveAllLinks(text))

    def Create(self, request, path):
      """Returns a new TemplateDataSource bound to |request|.
      """
      branch_info = self._branch_info.copy()
      branch_info['showWarning'] = (not path.startswith('apps') and
                                    branch_info['showWarning'])
      return TemplateDataSource(
          branch_info,
          self._api_data_source_factory.Create(request),
          self._api_list_data_source_factory.Create(),
          self._intro_data_source_factory.Create(),
          self._samples_data_source_factory.Create(request),
          self._known_issues_data_source,
          self._sidenav_data_source_factory.Create(path),
          self._cache,
          self._public_template_path,
          self._private_template_path,
          self._static_resources,
          request)

  def __init__(self,
               branch_info,
               api_data_source,
               api_list_data_source,
               intro_data_source,
               samples_data_source,
               known_issues_data_source,
               sidenav_data_source,
               cache,
               public_template_path,
               private_template_path,
               static_resources,
               request):
    self._branch_info = branch_info
    self._api_list_data_source = api_list_data_source
    self._intro_data_source = intro_data_source
    self._samples_data_source = samples_data_source
    self._api_data_source = api_data_source
    self._known_issues_data_source = known_issues_data_source
    self._sidenav_data_source = sidenav_data_source
    self._cache = cache
    self._public_template_path = public_template_path
    self._private_template_path = private_template_path
    self._static_resources = static_resources
    self._request = request

  def Render(self, template_name):
    """This method will render a template named |template_name|, fetching all
    the partial templates needed from |self._cache|. Partials are retrieved
    from the TemplateDataSource with the |get| method.
    """
    template = self.GetTemplate(self._public_template_path, template_name)
    if not template:
      return ''
      # TODO error handling
    render_data = template.render({
      'api_list': self._api_list_data_source,
      'apis': self._api_data_source,
      'branchInfo': self._branch_info,
      'intros': self._intro_data_source,
      'known_issues': self._known_issues_data_source,
      'sidenavs': self._sidenav_data_source,
      'partials': self,
      'samples': self._samples_data_source,
      'static': self._static_resources,
      'apps_title': 'Apps',
      'extensions_title': 'Extensions',
      'apps_samples_url': url_constants.GITHUB_BASE,
      'extensions_samples_url': url_constants.EXTENSIONS_SAMPLES,
      'true': True,
      'false': False
    })
    if render_data.errors:
      logging.error('Handlebar error(s) rendering %s:\n%s' %
          (template_name, '  \n'.join(render_data.errors)))
    return render_data.text

  def get(self, key):
    return self.GetTemplate(self._private_template_path, key)

  def GetTemplate(self, base_path, template_name):
    real_path = FormatKey(template_name)
    try:
      return self._cache.GetFromFile(base_path + '/' + real_path)
    except FileNotFoundError as e:
      logging.info(e)
      return None
