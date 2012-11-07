# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Paths to common resources in the Chrome repository."""

import os


_THIS_DIR = os.path.abspath(os.path.dirname(__file__))


def GetSrc():
  """Returns the path to the root src directory."""
  return os.path.join(_THIS_DIR, os.pardir, os.pardir, os.pardir, os.pardir)


def GetTestData():
  """Returns the path to the src/chrome/test/data directory."""
  return os.path.join(GetSrc(), 'chrome', 'test', 'data')


def GetThirdParty():
  """Returns the path to the src/third_party directory."""
  return os.path.join(GetSrc(), 'third_party')
