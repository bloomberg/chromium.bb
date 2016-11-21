#!/usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Checks that the host environment has the necessary packages installed to
build chrome.
"""

import subprocess
import sys

if sys.platform.startswith('linux'):
  if subprocess.call(['src/build/install-build-deps.sh', '--quick-check']):
    print ("Missing build dependencies. "
           "Try running 'build/install-build-deps.sh'")
    exit(1)
