# Copyright (c) 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os


def GetChromiumSrcDir():
  return os.path.abspath(os.path.join(
      os.path.dirname(__file__), os.pardir, os.pardir, os.pardir, os.pardir))
