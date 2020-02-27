# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the build_api script covering the base Build API functionality."""

from __future__ import print_function

import os
import sys

from google.protobuf import json_format

from chromite.api import api_config
from chromite.api import message_util
from chromite.api import router
from chromite.api.gen.chromite.api import build_api_test_pb2
from chromite.lib import chroot_lib
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


assert sys.version_info >= (3, 6), 'This module requires Python 3.6+'


class RouterTest(cros_test_lib.RunCommandTempDirTestCase,
                 api_config.ApiConfigMixin):
  """Test Router functionality."""

  def setUp(self):
    self.router = router.Router()
    self.router.Register(build_api_test_pb2)

    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    chroot_tmp = os.path.join(self.chroot_dir, 'tmp')
    # Make the tmp dir for the re-exec inside chroot input/output files.
    osutils.SafeMakedirs(chroot_tmp)

    # Build the input/output/config paths we'll be using in the tests.
    self.json_input_file = os.path.join(self.tempdir, 'input.json')
    self.json_output_file = os.path.join(self.tempdir, 'output.json')
    self.json_config_file = os.path.join(self.tempdir, 'config.json')
    self.binary_input_file = os.path.join(self.tempdir, 'input.bin')
    self.binary_output_file = os.path.join(self.tempdir, 'output.bin')
    self.binary_config_file = os.path.join(self.tempdir, 'config.bin')

    # The message handlers for the respective files.
    self.json_input_handler = message_util.get_message_handler(
        self.json_input_file, message_util.FORMAT_JSON)
    self.json_output_handler = message_util.get_message_handler(
        self.json_output_file, message_util.FORMAT_JSON)
    self.json_config_handler = message_util.get_message_handler(
        self.json_config_file, message_util.FORMAT_JSON)
    self.binary_input_handler = message_util.get_message_handler(
        self.binary_input_file, message_util.FORMAT_BINARY)
    self.binary_output_handler = message_util.get_message_handler(
        self.binary_output_file, message_util.FORMAT_BINARY)
    self.binary_config_handler = message_util.get_message_handler(
        self.binary_config_file, message_util.FORMAT_BINARY)

    # Build an input message to use.
    self.expected_id = 'input id'
    input_msg = build_api_test_pb2.TestRequestMessage()
    input_msg.id = self.expected_id
    input_msg.chroot.path = self.chroot_dir

    # Write out base input and config messages.
    osutils.WriteFile(self.json_input_file,
                      json_format.MessageToJson(input_msg))
    osutils.WriteFile(
        self.binary_input_file, input_msg.SerializeToString(), mode='wb')

    config_msg = self.api_config.get_proto()
    osutils.WriteFile(self.json_config_file,
                      json_format.MessageToJson(config_msg))
    osutils.WriteFile(
        self.binary_config_file, config_msg.SerializeToString(), mode='wb')

    self.subprocess_tempdir = os.path.join(self.chroot_dir, 'tempdir')
    osutils.SafeMakedirs(self.subprocess_tempdir)

  def testJsonInputOutputMethod(self):
    """Test json input/output handling."""
    def impl(input_msg, output_msg, config):
      self.assertIsInstance(input_msg, build_api_test_pb2.TestRequestMessage)
      self.assertIsInstance(output_msg, build_api_test_pb2.TestResultMessage)
      self.assertIsInstance(config, api_config.ApiConfig)
      self.assertEqual(config, self.api_config)

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route(
        'chromite.api.TestApiService',
        'InputOutputMethod',
        self.api_config,
        self.json_input_handler,
        [self.json_output_handler],
        self.json_config_handler)

  def testBinaryInputOutputMethod(self):
    """Test binary input/output handling."""
    def impl(input_msg, output_msg, config):
      self.assertIsInstance(input_msg, build_api_test_pb2.TestRequestMessage)
      self.assertIsInstance(output_msg, build_api_test_pb2.TestResultMessage)
      self.assertIsInstance(config, api_config.ApiConfig)
      self.assertEqual(config, self.api_config)

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route(
        'chromite.api.TestApiService',
        'InputOutputMethod',
        self.api_config,
        self.binary_input_handler,
        [self.binary_output_handler],
        self.binary_config_handler)

  def testMultipleOutputHandling(self):
    """Test multiple output handling."""
    expected_result = 'Success!'

    def impl(input_msg, output_msg, config):
      self.assertIsInstance(input_msg, build_api_test_pb2.TestRequestMessage)
      self.assertIsInstance(output_msg, build_api_test_pb2.TestResultMessage)
      self.assertIsInstance(config, api_config.ApiConfig)
      self.assertEqual(config, self.api_config)
      # Set the property on the output to test against.
      output_msg.result = expected_result

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route(
        'chromite.api.TestApiService',
        'InputOutputMethod',
        self.api_config,
        self.binary_input_handler,
        [self.binary_output_handler, self.json_output_handler],
        self.binary_config_handler)

    # Make sure it did write out all the expected files.
    self.assertExists(self.binary_output_file)
    self.assertExists(self.json_output_file)

    # Parse the output files back into a message.
    binary_msg = build_api_test_pb2.TestResultMessage()
    json_msg = build_api_test_pb2.TestResultMessage()
    self.binary_output_handler.read_into(binary_msg)
    self.json_output_handler.read_into(json_msg)

    # Make sure the parsed messages have the expected content.
    self.assertEqual(binary_msg.result, expected_result)
    self.assertEqual(json_msg.result, expected_result)

  def testRenameMethod(self):
    """Test implementation name config."""
    def _GetMethod(_, method_name):
      self.assertEqual('CorrectName', method_name)
      return lambda x, y, z: None

    self.PatchObject(self.router, '_GetMethod', side_effect=_GetMethod)

    self.router.Route('chromite.api.TestApiService', 'RenamedMethod',
                      self.api_config, self.binary_input_handler,
                      [self.binary_output_handler], self.binary_config_handler)

  def _mock_callable(self, expect_called):
    """Helper to create the implementation mock to test chroot assertions.

    Args:
      expect_called (bool): Whether the implementation should be called.
       When False, an assertion will fail if it is called.

    Returns:
      callable - The implementation.
    """
    def impl(_input_msg, _output_msg, _config):
      self.assertTrue(expect_called,
                      'The implementation should not have been called.')

    return impl

  def _writeChrootCallOutput(self, content='{}', mode='w'):
    def impl(*_args, **_kwargs):
      """Side effect for inside-chroot calls to the API."""
      osutils.WriteFile(
          os.path.join(self.subprocess_tempdir,
                       router.Router.REEXEC_OUTPUT_FILE),
          content,
          mode=mode)

    return impl

  def testInsideServiceInsideMethodInsideChroot(self):
    """Test inside/inside/inside works correctly."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.router.Route('chromite.api.InsideChrootApiService',
                      'InsideServiceInsideMethod', self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

  def testInsideServiceOutsideMethodOutsideChroot(self):
    """Test the outside method override works as expected."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.router.Route('chromite.api.InsideChrootApiService',
                      'InsideServiceOutsideMethod', self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

  def testInsideServiceInsideMethodOutsideChroot(self):
    """Test calling an inside method from outside the chroot."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)

    service = 'chromite.api.InsideChrootApiService'
    method = 'InsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

  def testInsideServiceOutsideMethodInsideChroot(self):
    """Test inside chroot for outside method raises an error."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.router.Route('chromite.api.InsideChrootApiService',
                        'InsideServiceOutsideMethod', self.api_config,
                        self.binary_input_handler, [self.binary_output_handler],
                        self.binary_config_handler)

  def testOutsideServiceOutsideMethodOutsideChroot(self):
    """Test outside/outside/outside works correctly."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.router.Route('chromite.api.OutsideChrootApiService',
                      'OutsideServiceOutsideMethod', self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

  def testOutsideServiceInsideMethodInsideChroot(self):
    """Test the inside method assertion override works properly."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.router.Route('chromite.api.OutsideChrootApiService',
                      'OutsideServiceInsideMethod', self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

  def testOutsideServiceInsideMethodOutsideChroot(self):
    """Test calling an inside override method from outside the chroot."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)

    service = 'chromite.api.OutsideChrootApiService'
    method = 'OutsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

  def testReexecNonemptyOutput(self):
    """Test calling an inside chroot method that produced output."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)

    # Patch the chroot tempdir method to return a tempdir with our subprocess
    # tempdir so the output file will be in the expected location.
    tempdir = osutils.TempDir()
    original = tempdir.tempdir
    tempdir.tempdir = self.subprocess_tempdir
    self.PatchObject(chroot_lib.Chroot, 'tempdir', return_value=tempdir)

    expected_output_msg = build_api_test_pb2.TestResultMessage()
    expected_output_msg.result = 'foo'

    # Set the command side effect to write out our expected output to the
    # output file for the inside the chroot reexecution of the endpoint.
    # This lets us make sure the logic moving everything out works as intended.
    self.rc.SetDefaultCmdResult(
        side_effect=self._writeChrootCallOutput(
            content=expected_output_msg.SerializeToString(), mode='wb'))

    service = 'chromite.api.OutsideChrootApiService'
    method = 'OutsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

    # It should be writing the result out to our output file.
    output_msg = build_api_test_pb2.TestResultMessage()
    self.binary_output_handler.read_into(output_msg)
    self.assertEqual(expected_output_msg, output_msg)

    tempdir.tempdir = original
    del tempdir

  def testReexecEmptyOutput(self):
    """Test calling an inside chroot method that produced no output."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    expected_output_msg = build_api_test_pb2.TestResultMessage()

    # Set the command side effect to write out our expected output to the
    # output file for the inside the chroot reexecution of the endpoint.
    # This lets us make sure the logic moving everything out works as intended.
    self.rc.SetDefaultCmdResult(
        side_effect=self._writeChrootCallOutput(
            content=expected_output_msg.SerializeToString(), mode='wb'))

    service = 'chromite.api.OutsideChrootApiService'
    method = 'OutsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

    output_msg = build_api_test_pb2.TestResultMessage()
    self.binary_output_handler.read_into(output_msg)
    self.assertEqual(expected_output_msg, output_msg)

  def testReexecNoOutput(self):
    """Test calling an inside chroot method that produced no output."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.rc.SetDefaultCmdResult(returncode=1)

    service = 'chromite.api.OutsideChrootApiService'
    method = 'OutsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.api_config,
                      self.binary_input_handler, [self.binary_output_handler],
                      self.binary_config_handler)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

    output_msg = build_api_test_pb2.TestResultMessage()
    empty_msg = build_api_test_pb2.TestResultMessage()
    self.binary_output_handler.read_into(output_msg)
    self.assertEqual(empty_msg, output_msg)

  def testInvalidService(self):
    """Test invalid service call."""
    service = 'chromite.api.DoesNotExist'
    method = 'OutsideServiceInsideMethod'

    with self.assertRaises(router.UnknownServiceError):
      self.router.Route(service, method, self.api_config,
                        self.binary_input_handler, [self.binary_output_handler],
                        self.binary_config_handler)

  def testInvalidMethod(self):
    """Test invalid method call."""
    service = 'chromite.api.OutsideChrootApiService'
    method = 'DoesNotExist'

    with self.assertRaises(router.UnknownMethodError):
      self.router.Route(service, method, self.api_config,
                        self.binary_input_handler, [self.binary_output_handler],
                        self.binary_config_handler)
