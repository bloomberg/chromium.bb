# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for the boto_compat module."""

from __future__ import print_function

import ConfigParser
import os

from chromite.lib import boto_compat
from chromite.lib import cros_test_lib


class FixBotoCertsTest(cros_test_lib.TestCase):
  """Tests FixBotoCerts functionality."""

  def testWrapper(self):
    with boto_compat.FixBotoCerts(strict=True):
      boto_paths = os.environ['BOTO_PATH'].split(':')
      self.assertIn('/etc/boto.cfg', boto_paths)

      fixcfg = boto_paths[-1]
      self.assertExists(fixcfg)

      config = ConfigParser.SafeConfigParser()
      config.read(fixcfg)
      cafile = config.get('Boto', 'ca_certificates_file')
      self.assertExists(cafile)
    self.assertNotExists(fixcfg)

  def testBotoConfigOverride(self):
    os.environ['BOTO_CONFIG'] = '/some/boto.cfg'
    with boto_compat.FixBotoCerts(strict=True):
      boto_paths = os.environ['BOTO_PATH'].split(':')
      self.assertIn('/some/boto.cfg', boto_paths)
      self.assertNotIn('BOTO_CONFIG', os.environ)
    self.assertEqual(os.environ['BOTO_CONFIG'], '/some/boto.cfg')

  def testActivateFalse(self):
    os.environ.pop('BOTO_PATH', None)
    with boto_compat.FixBotoCerts(strict=True, activate=False):
      self.assertNotIn('BOTO_PATH', os.environ)
