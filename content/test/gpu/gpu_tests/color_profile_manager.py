# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess
import sys

IMPL_PY = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(
  __file__))), 'utilities', 'color_profile_manager_impl.py')

def ForceUntilExitSRGB(skip_restoring_color_profile=False):
  if not sys.platform.startswith('darwin'):
    return
  # The Mac-specific Python packages used by the other scripts are only
  # available via the system-installed Python.
  subprocess.call(['/usr/bin/python', IMPL_PY])
