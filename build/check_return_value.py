#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program wraps an arbitrary command and prints "1" if the command ran
successfully."""

import subprocess
import sys

if not subprocess.call(sys.argv[1:]):
  print 1
else:
  print 0
