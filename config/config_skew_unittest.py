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

# Builders that we generally leave out of old versus new config comparisons
# as these builders existing in new config have no equivalent in the old
# config world.
POSTSUBMIT_EXCLUDE_BUILDERS = ["chromite-postsubmit", "test_vm-postsubmit"]

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

  def _get_new_config_children(self, name, exclude=None):
    """Returns the children for a builder config in new config.

    Args:
      name: string, name of builder to return the children of.
      exclude: optional list, names of children to exclude.
    """
    config = self._get_new_config(name)
    return [c for c in config.orchestrator.children if c not in exclude]

  def _get_old_config(self, name):
    return self.old_configs[name]

  def _get_old_config_slaves(self, name):
    config = self.old_configs[name]
    return config.slave_configs

  def _to_utf8(self, strings):
    return [string.decode("UTF-8") for string in strings]

  @cros_test_lib.ConfigSkewTest()
  def testPostsubmitBuildTargets(self):
    master_postsubmit_children = self._to_utf8(
        self._get_old_config_slaves("master-postsubmit"))
    # Exclude the special builders that old config is not expected to have.
    postsubmit_orchestrator_children = self._get_new_config_children(
        "postsubmit-orchestrator", POSTSUBMIT_EXCLUDE_BUILDERS)

    self.assertItemsEqual(postsubmit_orchestrator_children,
                          master_postsubmit_children)

  @cros_test_lib.ConfigSkewTest()
  def testPostsubmitBuildTargetsCriticality(self):
    # Exclude the special builders that old config is not expected to have.
    for child_name in self._get_new_config_children(
        "postsubmit-orchestrator", POSTSUBMIT_EXCLUDE_BUILDERS):
      new_config = self._get_new_config(child_name)
      old_config = self._get_old_config(child_name)
      # old_config doesn't exist is caught in another test, don't report here.
      new_critical = new_config.general.critical.value
      old_critical = old_config.important
      if old_config:
        self.assertEquals(
            new_critical, old_critical,
            "%s new (%s) and old (%s) configs did not match on criticality."
            % (child_name, new_critical, old_critical))
