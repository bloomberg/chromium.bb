#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
import os
import sys

sys.path.append(os.path.join(os.path.dirname(__file__),
    os.pardir, os.pardir, os.pardir, 'tools', 'telemetry'))

from telemetry import test_runner

if __name__ == '__main__':
  sys.exit(test_runner.Main())
