#!/usr/bin/env python
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
We need to move forked directories around on each pull, for Android on 1364.
"""

import os
import shutil

def main():
  if not os.environ.get('BUILDBOT_SLAVENAME','').startswith('droid')
    return 0
  forked = ['src/cc', 'src/webkit/compositor_bindings']
  ret = 0
  for fork in forked:
    shutil.rmtree(fork)
    shutil.move(fork + '_2', fork)
  return 0

if __name__ == '__main__':
  sys.exit(main())
