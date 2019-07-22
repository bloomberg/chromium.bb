# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for the validate module."""

from __future__ import print_function

import os

from chromite.api import api_config
from chromite.api import validate
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import osutils


class ExistsTest(cros_test_lib.TempDirTestCase):
  """Tests for the exists validator."""

  def test_not_exists(self):
    """Test the validator fails when given a path that doesn't exist."""
    path = os.path.join(self.tempdir, 'DOES_NOT_EXIST')

    @validate.exists('path')
    def impl(_input_proto):
      self.fail('Incorrectly allowed method to execute.')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot(path=path))

  def test_exists(self):
    """Test the validator fails when given a path that doesn't exist."""
    path = os.path.join(self.tempdir, 'chroot')
    osutils.SafeMakedirs(path)

    @validate.exists('path')
    def impl(_input_proto):
      pass

    impl(common_pb2.Chroot(path=path))


class IsInTest(cros_test_lib.TestCase):
  """Tests for the is_in validator."""

  def test_in(self):
    """Test a valid value."""
    @validate.is_in('path', ['/chroot/path', '/other/chroot/path'])
    def impl(*_args):
      pass

    # Make sure all of the values work.
    impl(common_pb2.Chroot(path='/chroot/path'))
    impl(common_pb2.Chroot(path='/other/chroot/path'))

  def test_not_in(self):
    """Test an invalid value."""
    @validate.is_in('path', ['/chroot/path', '/other/chroot/path'])
    def impl(*_args):
      pass

    # Should be failing on the invalid value.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot(path='/bad/value'))

  def test_not_set(self):
    """Test an unset value."""
    @validate.is_in('path', ['/chroot/path', '/other/chroot/path'])
    def impl(*_args):
      pass

    # Should be failing without a value set.
    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot())


class RequiredTest(cros_test_lib.TestCase):
  """Tests for the required validator."""

  def test_invalid_field(self):
    """Test validator fails when given an unset value."""

    @validate.require('does.not.exist')
    def impl(_input_proto):
      self.fail('Incorrectly allowed method to execute.')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot())

  def test_not_set(self):
    """Test validator fails when given an unset value."""

    @validate.require('env.use_flags')
    def impl(_input_proto):
      self.fail('Incorrectly allowed method to execute.')

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot())

  def test_set(self):
    """Test validator passes when given set values."""

    @validate.require('path', 'env.use_flags')
    def impl(_input_proto):
      pass

    impl(common_pb2.Chroot(path='/chroot/path',
                           env={'use_flags': [{'flag': 'test'}]}))

  def test_mixed(self):
    """Test validator passes when given a set value."""

    @validate.require('path', 'env.use_flags')
    def impl(_input_proto):
      pass

    with self.assertRaises(cros_build_lib.DieSystemExit):
      impl(common_pb2.Chroot(path='/chroot/path'))


class ValidateOnlyTest(cros_test_lib.TestCase, api_config.ApiConfigMixin):
  """validate_only decorator tests."""

  def test_validate_only(self):
    """Test validate only."""
    @validate.require('path')
    @validate.validation_complete
    def impl(_input_proto, _output_proto, _config):
      self.fail('Implementation was called.')
      return 1

    # Just using arbitrary messages, we just need the
    # (request, response, config) arguments so it can check the config.
    rc = impl(common_pb2.Chroot(path='/chroot/path'), common_pb2.Chroot(),
              self.validate_only_config)

    self.assertEqual(0, rc)

  def test_no_validate_only(self):
    """Test no use of validate only."""
    @validate.validation_complete
    def impl(_input_proto, _output_proto, _config):
      assert False

    # We will get an assertion error unless validate_only prevents the function
    # from being called.
    with self.assertRaises(AssertionError):
      impl(common_pb2.Chroot(), common_pb2.Chroot(), self.api_config)
