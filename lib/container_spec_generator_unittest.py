# -*- coding: utf-8 -*-
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for container_spec_generator."""

from __future__ import print_function

import copy
import mock

from chromite.lib import cros_test_lib
from chromite.lib import container_spec_generator
from chromite.lib import user_db

MOCK_UID = 1000
MOCK_GID = 1001

CONTAINER_NAME = 'container-name'

SIMPLE_MANIFEST_IMAGE_NAME = u'image-name'
SIMPLE_MANIFEST_TCP_PORT = 4000
SIMPLE_MANIFEST_EXEC_VALUE = [u'/bin/true', u'--quiet']

# Calls to set information in the wrapper that corresponds to SIMPLE_MANIFEST.
SIMPLE_MANIFEST_MOCK_WRAPPER_CALLS = [
    mock.call.AddExecutable(
        MOCK_UID, MOCK_GID,
        SIMPLE_MANIFEST_EXEC_VALUE,
        container_spec_generator.PortSpec(False, [SIMPLE_MANIFEST_TCP_PORT]),
        container_spec_generator.PortSpec(False, [])),
    mock.call.SetName(CONTAINER_NAME),
]

SIMPLE_MANIFEST = {
    'apps': [
        {
            'name': u'app-name',
            'image': {
                'name': SIMPLE_MANIFEST_IMAGE_NAME,
            },
            'app': {
                'exec': SIMPLE_MANIFEST_EXEC_VALUE,
                'user': u'chronos',
                'group': u'chronos',
                'workingDirectory': u'/opt/work',
                'ports': [
                    {
                        'name': u'health',
                        'port': SIMPLE_MANIFEST_TCP_PORT,
                        'protocol': u'tcp',
                        'socketActivated': False
                    }
                ]
            },
        },
    ],
}


class ContainerSpecGeneratorTest(cros_test_lib.MockTempDirTestCase):
  """Tests for chromite.lib.container_spec_generator."""

  def setUp(self):
    """Set up a test environment."""
    self._mock_wrapper = mock.Mock()
    self.PatchObject(user_db.UserDB, 'ResolveUsername', return_value=MOCK_UID)
    self.PatchObject(user_db.UserDB, 'ResolveGroupname', return_value=MOCK_GID)
    self.PatchObject(user_db.UserDB, 'UserExists', return_value=True)
    self.PatchObject(user_db.UserDB, 'GroupExists', return_value=True)
    self.PatchObject(container_spec_generator, 'ContainerSpecWrapper',
                     return_value=self._mock_wrapper)
    self._generator = container_spec_generator.ContainerSpecGenerator(
        self.tempdir)


  def testChecksForAtLeastOneExecutable(self):
    """Check that we'll throw up on manifests that don't name an executable."""
    with self.assertRaises(ValueError):
      self._generator.GetContainerSpec({u'apps': []}, CONTAINER_NAME)

  def testParsesSimpleManifest(self):
    """Test that we can correctly parse a simple manifest."""
    self._generator.GetContainerSpec(SIMPLE_MANIFEST, CONTAINER_NAME)
    self.assertEqual(SIMPLE_MANIFEST_MOCK_WRAPPER_CALLS,
                     self._mock_wrapper.mock_calls)

  def testAcNameTypeChecking(self):
    """Test that we validate name fields as being Ac Name Types."""
    with self.assertRaises(ValueError):
      manifest = copy.deepcopy(SIMPLE_MANIFEST)
      manifest['apps'][0]['name'] = u'我喜欢乌龟'
      self._generator.GetContainerSpec(manifest, CONTAINER_NAME)
    with self.assertRaises(ValueError):
      manifest = copy.deepcopy(SIMPLE_MANIFEST)
      manifest['apps'][0]['image']['name'] = u'我是一只猫'
      self._generator.GetContainerSpec(manifest, CONTAINER_NAME)
