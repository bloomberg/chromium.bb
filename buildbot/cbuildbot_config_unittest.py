#!/usr/bin/python

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for config.  Needs to be run inside of chroot for mox."""

import mox
import sys
import unittest

import constants
sys.path.append(constants.SOURCE_ROOT)
import chromite.buildbot.cbuildbot_config as config

class CBuildBotTest(mox.MoxTestBase):

  def setUp(self):
    mox.MoxTestBase.setUp(self)

  def testConfigUseflags(self):
    """ Useflags must be lists.
        Strings are interpreted as arrays of characters for this, which is not
        useful.
    """

    for x in config.config:
      useflags = config.config[x].get('useflags')
      if not useflags is None:
        self.assertTrue(
          isinstance(useflags, list),
          'Config for %s: useflags should be a list.' % x)

if __name__ == '__main__':
  unittest.main()
