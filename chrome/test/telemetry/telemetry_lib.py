# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import absolute_import

import os
import sys

path = os.path.join(os.path.dirname(__file__),
                    '..', '..', '..', 'tools', 'telemetry')
sys.path.append(path)
import telemetry
sys.path.pop()
