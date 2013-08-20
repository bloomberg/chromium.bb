# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from strings_data_source import StringsDataSource

class DataSourceRegistry(object):
  '''Collects all DataSources and creates a context for templates to be rendered
  in. All DataSources must conform to the DataSource interface.
  '''
  @staticmethod
  def AsTemplateData(server_instance):
    '''Return a context that can be used to render templates.
    '''
    registry = {
      'strings': StringsDataSource
    }

    return dict(
        (name, cls(server_instance)) for name, cls in registry.iteritems())
