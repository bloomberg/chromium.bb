# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

import logging
import os
import sys

def EnableLogging(name):
  '''Returns the output of the log with |name| to stdout.
  '''
  return _ReplaceLogging(name, lambda message: print(message))

def DisableLogging(name):
  '''Disables the log with |name| for the duration of the decorated function.
  '''
  return _ReplaceLogging(name, lambda _: None)

def _ReplaceLogging(name, replacement):
  def decorator(fn):
    def impl(*args, **optargs):
      saved = getattr(logging, name)
      setattr(logging, name, replacement)
      try:
        return fn(*args, **optargs)
      finally:
        setattr(logging, name, saved)
    return impl
  return decorator

# TODO(kalman): Use this everywhere. A lot of tests are doing this.
def ReadFile(name):
  with open(os.path.join(sys.path[0], os.pardir, os.pardir, name)) as f:
    return f.read()
