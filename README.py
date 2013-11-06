#!/usr/bin/env python
# coding=utf-8
# Copyright 2012 The Swarming Authors. All rights reserved.
# Use of this source code is governed under the Apache License, Version 2.0 that
# can be found in the LICENSE file.

###
# Run me to generate the documentation!
###

# Line too long (NN/80)
# pylint: disable=C0301

"""Test tracing and isolation infrastructure.

See googletest/*.py for scripts specifically managing GTest executables. More
information about googletest can be found at
http://code.google.com/p/googletest/wiki/Primer

A few scripts have strict dependency rules:
- The pure tracing scripts (trace_*.py) do not know about isolate
  infrastructure.
- Scripts outside googletest/ do not know about GTest.
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
