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

  def Render(self, template, request, data_sources=None, warn=True):
    '''Renders |template| using |request|.
    Specify |data_sources| to only include the DataSources with the given names
    when rendering the template.
    Specify |warn| whether to warn (e.g. logging.warning) if there are template
    rendering errors. It may be useful to disable this when errors are expected,
    for example when doing an initial page render to determine document title.
    '''
    assert isinstance(template, Handlebar), type(template)
    render_context = self._CreateDataSources(request)
    if data_sources is not None:
      render_context = dict((name, d) for name, d in render_context.iteritems()
                            if name in data_sources)
    render_context.update({
      'apps_samples_url': GITHUB_BASE,
      'base_path': self._server_instance.base_path,
      'extensions_samples_url': EXTENSIONS_SAMPLES,
      'static': self._server_instance.base_path + 'static',
    })
    render_data = template.Render(render_context)
    if warn and render_data.errors:
      logging.error('Handlebar error(s) rendering %s:\n%s' %
          (template._name, '  \n'.join(render_data.errors)))
    return render_data.text

  def _CreateDataSources(self, request):
    server_instance = self._server_instance
    data_sources = CreateDataSources(server_instance, request=request)
    data_sources.update({
      'api_list': server_instance.api_list_data_source_factory.Create(),
      'apis': server_instance.api_data_source_factory.Create(request),
      'samples': server_instance.samples_data_source_factory.Create(request),
    })
    return data_sources
