# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def EnsureDirectoryExists(dir_path):
  try:
    os.makedirs(dir_path)
  except OSError:
    pass


