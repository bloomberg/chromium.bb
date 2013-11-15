# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

from data_source_registry import CreateDataSources
from third_party.handlebar import Handlebar
from url_constants import GITHUB_BASE, EXTENSIONS_SAMPLES


class TemplateRenderer(object):
  '''Renders templates with the server's available data sources.
  '''

  def __init__(self, server_instance):
    self._server_instance = server_instance

  def Render(self, template, request):
    assert isinstance(template, Handlebar), type(template)
    server_instance = self._server_instance
    render_context = {
      'api_list': server_instance.api_list_data_source_factory.Create(),
      'apis': server_instance.api_data_source_factory.Create(request),
      'apps_samples_url': GITHUB_BASE,
      'base_path': server_instance.base_path,
      'extensions_samples_url': EXTENSIONS_SAMPLES,
      'false': False,
      'samples': server_instance.samples_data_source_factory.Create(request),
      'static': server_instance.base_path + 'static',
      'true': True,
    }
    render_context.update(CreateDataSources(server_instance, request=request))
    render_data = template.render(render_context)
    if render_data.errors:
      logging.error('Handlebar error(s) rendering %s:\n%s' %
          (template._name, '  \n'.join(render_data.errors)))
    return render_data.text
