# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import print_function

from chromite.api import build_api
from chromite.api.gen import build_api_test_pb2
from chromite.lib import cros_test_lib


class RouterTest(cros_test_lib.MockTestCase):
  """Test Router functionality."""
  _INPUT_JSON = '{"id":"Input ID"}'

  def setUp(self):
    self.router = build_api.Router()
    self.router.Register(build_api_test_pb2)

  def testInputOutputMethod(self):
    """Test input/output handling."""
    def impl(input_msg, output_msg):
      self.assertIsInstance(input_msg, build_api_test_pb2.TestRequestMessage)
      self.assertIsInstance(output_msg, build_api_test_pb2.TestResultMessage)

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route('chromite.api.TestApiService', 'InputOutputMethod',
                      self._INPUT_JSON)

    # Implementation expecting two methods with definitions that have Empty
    # for the input, output, and both messages.
    with self.assertRaises(TypeError):
      self.router.Route('chromite.api.TestApiService', 'InputMethod',
                        self._INPUT_JSON)
    with self.assertRaises(TypeError):
      self.router.Route('chromite.api.TestApiService', 'OutputMethod',
                        self._INPUT_JSON)
    with self.assertRaises(TypeError):
      self.router.Route('chromite.api.TestApiService', 'NoIoMethod',
                        self._INPUT_JSON)

  def testInputMethod(self):
    """Test methods with only an input message."""
    def impl(input_msg):
      self.assertIsInstance(input_msg, build_api_test_pb2.TestRequestMessage)

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route('chromite.api.TestApiService', 'InputMethod',
                      self._INPUT_JSON)

    # Test errors raised for implementations expecting input message with
    # definitions that expect both messages, output only, and neither.
    with self.assertRaises(TypeError):
      self.router.Route('chromite.api.TestApiService', 'InputOutputMethod',
                        self._INPUT_JSON)
    with self.assertRaises(AssertionError):
      self.router.Route('chromite.api.TestApiService', 'OutputMethod',
                        self._INPUT_JSON)
    with self.assertRaises(TypeError):
      self.router.Route('chromite.api.TestApiService', 'NoIoMethod',
                        self._INPUT_JSON)

  def testOutputMethod(self):
    """Test methods with only an output message."""
    def impl(output_msg):
      self.assertIsInstance(output_msg, build_api_test_pb2.TestResultMessage)

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route('chromite.api.TestApiService', 'OutputMethod',
                      self._INPUT_JSON)

    # Test errors raised for implementations expecting input message with
    # definitions that expect both messages, output only, and neither.
    with self.assertRaises(TypeError):
      self.router.Route('chromite.api.TestApiService', 'InputOutputMethod',
                        self._INPUT_JSON)
    with self.assertRaises(AssertionError):
      self.router.Route('chromite.api.TestApiService', 'InputMethod',
                        self._INPUT_JSON)
    with self.assertRaises(TypeError):
      self.router.Route('chromite.api.TestApiService', 'NoIoMethod',
                        self._INPUT_JSON)
