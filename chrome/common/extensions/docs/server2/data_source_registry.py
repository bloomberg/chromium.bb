# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from manifest_data_source import ManifestDataSource
from sidenav_data_source import SidenavDataSource
from strings_data_source import StringsDataSource

_all_data_sources = {
  'manifest_source': ManifestDataSource,
  'sidenavs': SidenavDataSource,
  'strings': StringsDataSource
}

def CreateDataSources(server_instance, request=None):
  '''Create a dictionary of initialized DataSources. DataSources are
  initialized with |server_instance| and |request|. If the DataSources are
  going to be used for Cron, |request| should be omitted.

  The key of each DataSource is the name the template system will use to access
  the DataSource.
  '''
  data_sources = {}
  for name, cls in _all_data_sources.iteritems():
    data_sources[name] = cls(server_instance, request)

  return data_sources
