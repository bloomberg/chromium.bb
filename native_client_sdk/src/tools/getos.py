#!/usr/bin/env python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Determine OS

Determine the name of the platform used to determine the correct Toolchain to
invoke.
"""

import sys

def GetPlatform():
  if sys.platform.startswith('cygwin') or sys.platform.startswith('win'):
    return 'win'

  if sys.platform.startswith('darwin'):
    return 'mac'

  if sys.platform.startswith('linux'):
    return 'linux'
  return None

if __name__ == '__main__':
  platform = GetPlatform()
  if platform is None:
    print 'Unknown platform.'
    sys.exit(1)

  print platform
  sys.exit(0)

