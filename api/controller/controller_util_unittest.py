# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""controller_util unittests."""

from __future__ import print_function

from chromite.api.controller import controller_util
from chromite.api.gen.chromiumos import common_pb2
from chromite.lib import cros_test_lib
from chromite.lib import portage_util


class CPVToPackageInfoTest(cros_test_lib.TestCase):
  """CPVToPackageInfo tests."""

  def testAllFields(self):
    pi = common_pb2.PackageInfo()
    cpv = portage_util.SplitCPV('cat/pkg-2.0.0', strict=False)

    controller_util.CPVToPackageInfo(cpv, pi)
    self.assertEqual('cat', pi.category)
    self.assertEqual('pkg', pi.package_name)
    self.assertEqual('2.0.0', pi.version)

  def testNoVersion(self):
    pi = common_pb2.PackageInfo()
    cpv = portage_util.SplitCPV('cat/pkg', strict=False)

    controller_util.CPVToPackageInfo(cpv, pi)
    self.assertEqual('cat', pi.category)
    self.assertEqual('pkg', pi.package_name)
    self.assertEqual('', pi.version)

  def testPackageOnly(self):
    pi = common_pb2.PackageInfo()
    cpv = portage_util.SplitCPV('pkg', strict=False)

    controller_util.CPVToPackageInfo(cpv, pi)
    self.assertEqual('', pi.category)
    self.assertEqual('pkg', pi.package_name)
    self.assertEqual('', pi.version)


class PackageInfoToCPVTest(cros_test_lib.TestCase):
  """PackageInfoToCPV tests."""

  def testAllFields(self):
    """Quick sanity check it's working properly."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'
    pi.category = 'cat'
    pi.version = '2.0.0'

    cpv = controller_util.PackageInfoToCPV(pi)

    self.assertEqual('pkg', cpv.package)
    self.assertEqual('cat', cpv.category)
    self.assertEqual('2.0.0', cpv.version)

  def testNoPackageInfo(self):
    """Test no package info given."""
    self.assertIsNone(controller_util.PackageInfoToCPV(None))

  def testNoPackageName(self):
    """Test no package name given."""
    pi = common_pb2.PackageInfo()
    pi.category = 'cat'
    pi.version = '2.0.0'

    self.assertIsNone(controller_util.PackageInfoToCPV(pi))


class PackageInfoToStringTest(cros_test_lib.TestCase):
  """PackageInfoToString tests."""

  def testAllFields(self):
    """Test all fields present."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'
    pi.category = 'cat'
    pi.version = '2.0.0'

    cpv_str = controller_util.PackageInfoToString(pi)

    self.assertEqual('cat/pkg-2.0.0', cpv_str)

  def testNoVersion(self):
    """Test no version provided."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'
    pi.category = 'cat'

    cpv_str = controller_util.PackageInfoToString(pi)

    self.assertEqual('cat/pkg', cpv_str)

  def testPackageOnly(self):
    """Test no version provided."""
    pi = common_pb2.PackageInfo()
    pi.package_name = 'pkg'

    cpv_str = controller_util.PackageInfoToString(pi)

    self.assertEqual('pkg', cpv_str)

  def testNoPackageName(self):
    """Test no package name given."""
    pi = common_pb2.PackageInfo()

    with self.assertRaises(ValueError):
      controller_util.PackageInfoToString(pi)


class CPVToStringTest(cros_test_lib.TestCase):
  """CPVToString tests."""

  def testTranslations(self):
    """Test standard translations used."""
    cases = [
        'cat/pkg-2.0.0-r1',
        'cat/pkg-2.0.0',
        'cat/pkg',
        'pkg',
    ]

    for case in cases:
      cpv = portage_util.SplitCPV(case, strict=False)
      # We should end up with as much info as is available, so we should see
      # the original value in each case.
      self.assertEqual(case, controller_util.CPVToString(cpv))

  def testInvalidCPV(self):
    """Test invalid CPV object."""
    cpv = portage_util.SplitCPV('', strict=False)
    with self.assertRaises(ValueError):
      controller_util.CPVToString(cpv)
