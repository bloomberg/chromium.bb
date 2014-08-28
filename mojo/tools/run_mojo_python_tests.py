#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

_script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(_script_dir, "pylib"))

from mojo_python_tests_runner import MojoPythonTestRunner


def main():
  runner = MojoPythonTestRunner(os.path.join('mojo', 'public', 'tools',
                                             'bindings', 'pylib'))
  sys.exit(runner.run())


if __name__ == '__main__':
  sys.exit(main())
