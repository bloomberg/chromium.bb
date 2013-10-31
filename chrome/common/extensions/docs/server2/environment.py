# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


def IsDevServer():
  return os.environ.get('SERVER_SOFTWARE', '').find('Development') == 0


def IsPreviewServer():
  return sys.argv and os.path.basename(sys.argv[0]) == 'preview.py'
