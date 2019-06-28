# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""API controller tests."""

from __future__ import print_function

from chromite.api import router
from chromite.api.controller import api as api_controller
from chromite.api.gen.chromite.api import api_pb2
from chromite.lib import cros_test_lib


class GetMethodsTest(cros_test_lib.MockTestCase):
  """GetMethods tests."""

  def testGetMethods(self):
    """Simple GetMethods sanity check."""
    methods = ['foo', 'bar']
    self.PatchObject(router.Router, 'ListMethods', return_value=methods)

    request = api_pb2.MethodGetRequest()
    response = api_pb2.MethodGetResponse()

    api_controller.GetMethods(request, response)

    self.assertItemsEqual(methods, [m.method for m in response.methods])


class GetVersionTest(cros_test_lib.MockTestCase):
  """GetVersion tests."""

  def testGetVersion(self):
    """Simple GetVersion sanity check."""
    self.PatchObject(api_controller, 'VERSION_MAJOR', new=1)
    self.PatchObject(api_controller, 'VERSION_MINOR', new=2)
    self.PatchObject(api_controller, 'VERSION_BUG', new=3)

    request = api_pb2.VersionGetRequest()
    response = api_pb2.VersionGetResponse()

    api_controller.GetVersion(request, response)

    self.assertEqual(response.version.major, 1)
    self.assertEqual(response.version.minor, 2)
    self.assertEqual(response.version.bug, 3)
