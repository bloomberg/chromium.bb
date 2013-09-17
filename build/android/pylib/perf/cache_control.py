# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class CacheControl(object):
  _DROP_CACHES = '/proc/sys/vm/drop_caches'

  def __init__(self, adb):
    self._adb = adb

  def DropRamCaches(self):
    """Drops the filesystem ram caches for performance testing."""
    self._adb.RunShellCommandWithSU('sync')
    self._adb.SetProtectedFileContents(CacheControl._DROP_CACHES, '3')

