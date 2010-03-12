#!/usr/bin/python
# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Simple utilities to assist with pyauto test scripts.

Assumes the test scripts live in chrome/test/functional.
"""

import os
import sys

# Add path to simplejson
sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..', '..', '..',
                             'third_party'))

# Add path to pyauto
sys.path.append(os.path.join(os.path.dirname(__file__),
                             '..',
                             'pyautolib'))
