# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from data_source import DataSource
from intro_data_source import IntroDataSource
from manifest_data_source import ManifestDataSource
from permissions_data_source import PermissionsDataSource
from sidenav_data_source import SidenavDataSource
from strings_data_source import StringsDataSource
from template_data_source import TemplateDataSource
from whats_new_data_source import WhatsNewDataSource


_all_data_sources = {
  'intros': IntroDataSource,
  'manifest_source': ManifestDataSource,
  'partials': TemplateDataSource,
  'permissions': PermissionsDataSource,
  'sidenavs': SidenavDataSource,
  'strings': StringsDataSource,
  'whatsNew' : WhatsNewDataSource
}

assert all(issubclass(cls, DataSource)
           for cls in _all_data_sources.itervalues())

def CreateDataSources(server_instance, request=None):
  '''Create a dictionary of initialized DataSources. DataSources are
  initialized with |server_instance| and |request|. If the DataSources are
  going to be used for Cron, |request| should be omitted.

  The key of each DataSource is the name the template system will use to access
  the DataSource.
  '''
  return dict((name, cls(server_instance, request))
              for name, cls in _all_data_sources.iteritems())
