#!/usr/bin/env python
# coding=utf-8
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

###
# Run me to generate the documentation!
###

"""Google Test specific tracing and isolation infrastructure.

You will find more general information in the directory above.
"""

import os
import sys


def main():
  for i in sorted(os.listdir(os.path.dirname(os.path.abspath(__file__)))):
    if not i.endswith('.py') or i == 'PRESUBMIT.py':
      continue
    module = __import__(i[:-3])
    if hasattr(module, '__doc__'):
      print module.__name__
      print ''.join('  %s\n' % i for i in module.__doc__.splitlines())
  return 0


if __name__ == '__main__':
  sys.exit(main())
