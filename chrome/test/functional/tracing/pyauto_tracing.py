#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

def _SetupPaths():
  """Setting path to find pyauto_functional.py."""
  tracing_dir = os.path.abspath(os.path.dirname(__file__))
  sys.path.append(tracing_dir)
  sys.path.append(os.path.normpath(os.path.join(tracing_dir, os.pardir)))

_SetupPaths()


from pyauto_functional import Main


if __name__ == '__main__':
  Main()
