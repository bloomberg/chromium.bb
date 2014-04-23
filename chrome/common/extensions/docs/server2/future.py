# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

_no_value = object()


def Collect(futures, except_pass=None):
  '''Creates a Future which returns a list of results from each Future in
  |futures|. |except_pass| should be one or more exceptions to ignore when
  calling Get on the futures.
  '''
  def resolve():
    resolved = []
    for f in futures:
      try:
        resolved.append(f.Get())
      # "except None" will simply not catch any errors
      except except_pass:
        pass
    return resolved
  return Future(callback=resolve)


class Future(object):
  '''Stores a value, error, or callback to be used later.
  '''
  def __init__(self, value=_no_value, callback=None, exc_info=None):
    self._value = value
    self._callback = callback
    self._exc_info = exc_info
    if (self._value is _no_value and
        self._callback is None and
        self._exc_info is None):
      raise ValueError('Must have either a value, error, or callback.')

  def Get(self):
    '''Gets the stored value, error, or callback contents.
    '''
    if self._value is not _no_value:
      return self._value
    if self._exc_info is not None:
      self._Raise()
    try:
      self._value = self._callback()
      return self._value
    except:
      self._exc_info = sys.exc_info()
      self._Raise()

  def _Raise(self):
    exc_info = self._exc_info
    raise exc_info[0], exc_info[1], exc_info[2]
