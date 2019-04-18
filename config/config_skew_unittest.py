# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for checking skew between old and new config worlds."""

from __future__ import print_function

import os

from chromite.api.gen.chromiumos import builder_config_pb2
from chromite.config import chromeos_config
from chromite.lib import constants
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from google.protobuf import json_format

BUILDER_CONFIG_FILENAME = os.path.join(
    constants.SOURCE_ROOT, 'infra/config/generated/builder_configs.cfg')

class ErrorWrapper(Exception):
  """Simple exception wrapper to provide more failure context."""

  def __init__(self, message, cause):
    super(ErrorWrapper, self).__init__(message + ", caused by " + repr(cause))
    self.cause = cause


class ConfigSkewTest(cros_test_lib.TestCase):
  """Tests checking if new config and legacy config are in sync."""

  def __init__(self, *args, **kwargs):
    super(ConfigSkewTest, self).__init__(*args, **kwargs)
    # This is loaded lazily, see self._get_new_configs().
    self._new_configs = None
    self.old_configs = chromeos_config.GetConfig()

  def _get_new_configs(self):
    """Lazily loads and returns the new configs.

    Lazily loads and returns the new approach configuration. This is
    done lazily because this configuration data is not available on
    builders sporting an external manifest. This test only executes
    the test methods if the parameter --config_skew is provided but
    it still constructs this class before the test methods become
    noops. Thus the lazy loading moving the load out of the constructor.
    """
    if self._new_configs is None:
      try:
        self._new_configs = json_format.Parse(
            osutils.ReadFile(BUILDER_CONFIG_FILENAME),
            builder_config_pb2.BuilderConfigs(),
            ignore_unknown_fields=True)
      except IOError as err:
        msg = "IOError, are you running this with an external manifest?"
        raise ErrorWrapper(msg, err)
    return self._new_configs.builder_configs

  def _get_new_config(self, name):
    for config in self._get_new_configs():
      if config.id.name == name:
        return config
    return None

  def _get_old_config(self, name):
    return self.old_configs[name]

  def _to_utf8(self, strings):
    return [string.decode("UTF-8") for string in strings]

  @cros_test_lib.ConfigSkewTest()
  def testPostsubmitBuildTargets(self):
    master_postsubmit_config = self._get_old_config("master-postsubmit")
    postsubmit_orchestrator_config = self._get_new_config(
        "postsubmit-orchestrator")

    master_postsubmit_children = self._to_utf8(
        master_postsubmit_config.slave_configs)
    postsubmit_orchestrator_children = (
        postsubmit_orchestrator_config.orchestrator.children)

    self.assertItemsEqual(postsubmit_orchestrator_children,
                          master_postsubmit_children)
