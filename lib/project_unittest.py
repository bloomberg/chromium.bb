# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the project library."""

from __future__ import print_function

import os

from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import project


class ProjectTest(cros_test_lib.TempDirTestCase):
  """Unittest for project.py"""

  def testLayoutFormat(self):
    content = {'repo-name': 'hello',
               'bar': 'foo'}
    project.WriteLayoutConf(content, self.tempdir)

    layout_conf = osutils.ReadFile(os.path.join(self.tempdir, 'metadata',
                                                'layout.conf')).split('\n')

    expected_lines = ['repo-name = hello',
                      'bar = foo',
                      'profile-formats = portage-2']
    for line in expected_lines:
      self.assertTrue(line in layout_conf)

  def testWriteParents(self):
    parents = ['hello:bonjour',
               'foo:bar']

    project.WriteParents(parents, self.tempdir)
    parents_content = osutils.ReadFile(os.path.join(self.tempdir, 'profiles',
                                                    'base', 'parent'))

    self.assertEqual('hello:bonjour\nfoo:bar', parents_content)

  def testConfigurationGenerated(self):
    sample_config = {'name': 'hello',
                     'dependencies': []}

    project.WriteProjectJson(sample_config, self.tempdir)

    self.assertExists(os.path.join(self.tempdir, 'profiles', 'base', 'parent'))
    self.assertExists(os.path.join(self.tempdir, 'metadata', 'layout.conf'))
