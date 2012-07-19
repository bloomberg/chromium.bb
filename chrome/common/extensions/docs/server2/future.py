# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

_no_value = object()

class Future(object):
  """Stores a value, error, or delegate to be used later.
  """
  def __init__(self, value=_no_value, error=None, delegate=None):
    self._value = value
    self._error = error
    self._delegate = delegate
    if (self._value is _no_value and
        self._error is None and
        self._delegate is None):
      raise ValueError('Must have either a value, error, or delegate.')

  def Get(self):
    """Gets the stored value, error, or delegate contents.
    """
    if self._value is not _no_value:
      return self._value
    if self._error is not None:
      raise self._error
    try:
      self._value = self._delegate.Get()
    except Exception as self._error:
      raise
    return self._value
