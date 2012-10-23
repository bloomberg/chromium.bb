#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Fake (similar in memory implementation) version of GSDStorage."""


import file_tools


class FakeStorage(object):
  """Testing replacement for GSDStorage that stores to memory."""

  def __init__(self):
    self._store = {}

  def PutData(self, data, key):
    self._store[key] = data

  def PutFile(self, path, key):
    self.PutData(file_tools.ReadFile(path), key)
    # Use form: fake://<key> for make-believe URLs.
    return 'fake://' + key

  def GetData(self, key):
    return self._store.get(key)

  def GetFile(self, key, path):
    data = self.GetData(key)
    if data is not None:
      file_tools.WriteFile(data, path)
      # Use form: fake://<key> for make-believe URLs.
      return 'fake://' + key
    else:
      return None

  def ItemCount(self):
    return len(self._store)
