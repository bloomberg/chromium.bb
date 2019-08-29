# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the build_api script covering the base Build API functionality."""

from __future__ import print_function

import os

from google.protobuf import json_format

from chromite.api import api_config
from chromite.api import router
from chromite.api.gen.chromite.api import build_api_test_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class RouterTest(cros_test_lib.RunCommandTempDirTestCase,
                 api_config.ApiConfigMixin):
  """Test Router functionality."""
  _INPUT_JSON_TEMPLATE = '{"id":"Input ID", "chroot":{"path": "%s"}}'

  def setUp(self):
    self.router = router.Router()
    self.router.Register(build_api_test_pb2)

    self.input_file = os.path.join(self.tempdir, 'input.json')
    self.output_file = os.path.join(self.tempdir, 'output.json')

    self.chroot_dir = os.path.join(self.tempdir, 'chroot')
    chroot_tmp = os.path.join(self.chroot_dir, 'tmp')
    # Make the tmp dir for the re-exec inside chroot input/output files.
    osutils.SafeMakedirs(chroot_tmp)

    osutils.WriteFile(self.input_file,
                      self._INPUT_JSON_TEMPLATE % self.chroot_dir)
    osutils.WriteFile(self.output_file, '{}')

    self.subprocess_tempdir = os.path.join(self.chroot_dir, 'tempdir')
    osutils.SafeMakedirs(self.subprocess_tempdir)
    self.PatchObject(osutils.TempDir, '__enter__',
                     return_value=self.subprocess_tempdir)

  def testInputOutputMethod(self):
    """Test input/output handling."""
    def impl(input_msg, output_msg, config):
      self.assertIsInstance(input_msg, build_api_test_pb2.TestRequestMessage)
      self.assertIsInstance(output_msg, build_api_test_pb2.TestResultMessage)
      self.assertIsInstance(config, api_config.ApiConfig)

    self.PatchObject(self.router, '_GetMethod', return_value=impl)

    self.router.Route('chromite.api.TestApiService', 'InputOutputMethod',
                      self.input_file, self.output_file, self.api_config)

  def testRenameMethod(self):
    """Test implementation name config."""
    def _GetMethod(_, method_name):
      self.assertEqual('CorrectName', method_name)
      return lambda x, y, z: None

    self.PatchObject(self.router, '_GetMethod', side_effect=_GetMethod)

    self.router.Route('chromite.api.TestApiService', 'RenamedMethod',
                      self.input_file, self.output_file, self.api_config)

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

  def _writeChrootCallOutput(self, content='{}'):
    def impl(*_args, **_kwargs):
      """Side effect for inside-chroot calls to the API."""
      osutils.WriteFile(
          os.path.join(self.subprocess_tempdir,
                       router.Router.REEXEC_OUTPUT_FILE),
          content)

    return impl

  def testInsideServiceInsideMethodInsideChroot(self):
    """Test inside/inside/inside works correctly."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.router.Route('chromite.api.InsideChrootApiService',
                      'InsideServiceInsideMethod', self.input_file,
                      self.output_file, self.api_config)

  def testInsideServiceOutsideMethodOutsideChroot(self):
    """Test the outside method override works as expected."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.router.Route('chromite.api.InsideChrootApiService',
                      'InsideServiceOutsideMethod', self.input_file,
                      self.output_file, self.api_config)

  def testInsideServiceInsideMethodOutsideChroot(self):
    """Test calling an inside method from outside the chroot."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.rc.SetDefaultCmdResult(side_effect=self._writeChrootCallOutput())

    service = 'chromite.api.InsideChrootApiService'
    method = 'InsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.input_file, self.output_file,
                      self.api_config)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

  def testInsideServiceOutsideMethodInsideChroot(self):
    """Test inside chroot for outside method raises an error."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    with self.assertRaises(cros_build_lib.DieSystemExit):
      self.router.Route('chromite.api.InsideChrootApiService',
                        'InsideServiceOutsideMethod', self.input_file,
                        self.output_file, self.api_config)

  def testOutsideServiceOutsideMethodOutsideChroot(self):
    """Test outside/outside/outside works correctly."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.router.Route('chromite.api.OutsideChrootApiService',
                      'OutsideServiceOutsideMethod', self.input_file,
                      self.output_file, self.api_config)

  def testOutsideServiceInsideMethodInsideChroot(self):
    """Test the inside method assertion override works properly."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=True))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=True)
    self.router.Route('chromite.api.OutsideChrootApiService',
                      'OutsideServiceInsideMethod', self.input_file,
                      self.output_file, self.api_config)

  def testOutsideServiceInsideMethodOutsideChroot(self):
    """Test calling an inside override method from outside the chroot."""
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.rc.SetDefaultCmdResult(side_effect=self._writeChrootCallOutput())

    service = 'chromite.api.OutsideChrootApiService'
    method = 'OutsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.input_file, self.output_file,
                      self.api_config)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

  def testReexecNonemptyOutput(self):
    """Test calling an inside chroot method that produced output."""
    osutils.WriteFile(self.output_file, '')
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.rc.SetDefaultCmdResult(
        side_effect=self._writeChrootCallOutput(content='{"result": "foo"}'))

    service = 'chromite.api.OutsideChrootApiService'
    method = 'OutsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.input_file, self.output_file,
                      self.api_config)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)

    # It should be writing the result out to our output file.
    expected = build_api_test_pb2.TestResultMessage()
    json_format.Parse('{"result": "foo"}', expected)
    result = build_api_test_pb2.TestResultMessage()
    json_format.Parse(osutils.ReadFile(self.output_file), result)
    self.assertEqual(expected, result)

  def testReexecEmptyOutput(self):
    """Test calling an inside chroot method that produced no output."""
    osutils.WriteFile(self.output_file, '')
    self.PatchObject(self.router, '_GetMethod',
                     return_value=self._mock_callable(expect_called=False))
    self.PatchObject(cros_build_lib, 'IsInsideChroot', return_value=False)
    self.rc.SetDefaultCmdResult(returncode=1)

    service = 'chromite.api.OutsideChrootApiService'
    method = 'OutsideServiceInsideMethod'
    service_method = '%s/%s' % (service, method)
    self.router.Route(service, method, self.input_file, self.output_file,
                      self.api_config)

    self.assertCommandContains(['build_api', service_method], enter_chroot=True)
    # It should be writing the empty message out.
    self.assertFileContents(self.output_file, '{}')

  def testInvalidService(self):
    """Test invalid service call."""
    service = 'chromite.api.DoesNotExist'
    method = 'OutsideServiceInsideMethod'

    with self.assertRaises(router.UnknownServiceError):
      self.router.Route(service, method, self.input_file, self.output_file,
                        self.api_config)

  def testInvalidMethod(self):
    """Test invalid method call."""
    service = 'chromite.api.OutsideChrootApiService'
    method = 'DoesNotExist'

    with self.assertRaises(router.UnknownMethodError):
      self.router.Route(service, method, self.input_file, self.output_file,
                        self.api_config)
