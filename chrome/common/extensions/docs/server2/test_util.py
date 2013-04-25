# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging

def DisableLogging(name):
  '''Disables the log with |name| for the duration of the decorated function.
  '''
  def decorator(fn):
    def impl(*args, **optargs):
      saved = getattr(logging, name)
      setattr(logging, name, lambda _: None)
      try:
        return fn(*args, **optargs)
      finally:
        setattr(logging, name, saved)
    return impl
  return decorator
