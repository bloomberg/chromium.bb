# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from manifest_data_source import ManifestDataSource
from strings_data_source import StringsDataSource

_all_data_sources = {
  'manifest_source': ManifestDataSource,
  'strings': StringsDataSource
}

def CreateDataSources(server_instance):
  '''Yields tuples of a name and an instantiated DataSource. The name is the
  string that templates use to access the DataSource. Each DataSource is
  initialized with |server_instance|.
  '''
  return dict(
    (name, cls(server_instance)) for name, cls in _all_data_sources.iteritems())
