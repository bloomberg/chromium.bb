# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

class DataSource(object):
  '''A DataSource presents data to the template system.

  DataSources are created with a ServerInstance. Anything in the ServerInstance
  can be used by the DataSource to create or modify data into a form more
  appropriate for usage by the template system.

  DataSources may not share data with other DataSources. The output of a
  DataSource is only useful to templates. Any data that must be shared, or is
  useful in general, should be moved into ServerInstance.
  '''
  def Cron(self):
    '''Must cache all files needed by |get| to persist them. Called on a live
    file system and can access files not in cache.
    '''
    raise NotImplementedError(self.__class__)

  def get(self, key):
    '''Returns a dictionary or list that can be consumed by a template. Called
    on an offline file system and can only access files in the cache.
    '''
    raise NotImplementedError(self.__class__)
