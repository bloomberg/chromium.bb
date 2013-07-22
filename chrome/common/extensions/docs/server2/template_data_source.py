# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from docs_server_utils import FormatKey
from file_system import FileNotFoundError
from third_party.handlebar import Handlebar
import url_constants

_EXTENSIONS_URL = '/chrome/extensions'

_STRING_CONSTANTS = {
  'app': 'app',
  'apps_title': 'Apps',
  'extension': 'extension',
  'extensions_title': 'Extensions',
  'events': 'events',
  'methods': 'methods',
  'properties': 'properties',
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
                 api_data_source_factory,
                 api_list_data_source_factory,
                 intro_data_source_factory,
                 samples_data_source_factory,
                 sidenav_data_source_factory,
                 compiled_fs_factory,
                 ref_resolver_factory,
                 manifest_data_source,
                 public_template_path,
                 private_template_path,
                 base_path):
      self._api_data_source_factory = api_data_source_factory
      self._api_list_data_source_factory = api_list_data_source_factory
      self._intro_data_source_factory = intro_data_source_factory
      self._samples_data_source_factory = samples_data_source_factory
      self._sidenav_data_source_factory = sidenav_data_source_factory
      self._cache = compiled_fs_factory.Create(self._CreateTemplate,
                                               TemplateDataSource)
      self._ref_resolver = ref_resolver_factory.Create()
      self._public_template_path = public_template_path
      self._private_template_path = private_template_path
      self._manifest_data_source = manifest_data_source
      self._base_path = base_path

    def _CreateTemplate(self, template_name, text):
      return Handlebar(self._ref_resolver.ResolveAllLinks(text))

    def Create(self, request, path):
      """Returns a new TemplateDataSource bound to |request|.
      """
      return TemplateDataSource(
          self._api_data_source_factory.Create(request),
          self._api_list_data_source_factory.Create(),
          self._intro_data_source_factory.Create(),
          self._samples_data_source_factory.Create(request),
          self._sidenav_data_source_factory.Create(path),
          self._cache,
          self._manifest_data_source,
          self._public_template_path,
          self._private_template_path,
          self._base_path)

  def __init__(self,
               api_data_source,
               api_list_data_source,
               intro_data_source,
               samples_data_source,
               sidenav_data_source,
               cache,
               manifest_data_source,
               public_template_path,
               private_template_path,
               base_path):
    self._api_list_data_source = api_list_data_source
    self._intro_data_source = intro_data_source
    self._samples_data_source = samples_data_source
    self._api_data_source = api_data_source
    self._sidenav_data_source = sidenav_data_source
    self._cache = cache
    self._public_template_path = public_template_path
    self._private_template_path = private_template_path
    self._manifest_data_source = manifest_data_source
    self._base_path = base_path

  def Render(self, template_name):
    """This method will render a template named |template_name|, fetching all
    the partial templates needed from |self._cache|. Partials are retrieved
    from the TemplateDataSource with the |get| method.
    """
    template = self.GetTemplate(self._public_template_path, template_name)
    if template is None:
      return None
      # TODO error handling
    render_data = template.render({
      'api_list': self._api_list_data_source,
      'apis': self._api_data_source,
      'intros': self._intro_data_source,
      'sidenavs': self._sidenav_data_source,
      'partials': self,
      'manifest_source': self._manifest_data_source,
      'samples': self._samples_data_source,
      'apps_samples_url': url_constants.GITHUB_BASE,
      'extensions_samples_url': url_constants.EXTENSIONS_SAMPLES,
      'static': self._base_path + '/static',
      'strings': _STRING_CONSTANTS,
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
    try:
      return self._cache.GetFromFile(
          '/'.join((base_path, FormatKey(template_name))))
    except FileNotFoundError:
      return None
