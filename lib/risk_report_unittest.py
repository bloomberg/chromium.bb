# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for risk_report.py"""

from __future__ import print_function

from chromite.lib import cros_test_lib
from chromite.lib import risk_report


class RiskReportTest(cros_test_lib.MockTestCase):
  """Test that the risk report works."""
  def __init__(self, *args):
    super(RiskReportTest, self).__init__(*args)
    self.risks = {}

  def setUp(self):
    self.conn = self.PatchObject(
        risk_report, '_GetCLRisks', lambda _id: self.risks)

  def testSingle(self):
    self.risks = {1: 0.0}
    self.assertEqual(
        risk_report.CLRiskText(42),
        'CL-Scanner risks: 1=0.0%')

  def testMultipleBelow(self):
    self.risks = {1: 0.05, 2: 0.03, 3: 0.01}
    self.assertEqual(
        risk_report.CLRiskText(42),
        'CL-Scanner risks: 1=5.0%')

  def testMultipleAbove(self):
    self.risks = {1: 0.5, 2: 0.4999, 3: 0.4998}
    self.assertEqual(
        risk_report.CLRiskText(42),
        'CL-Scanner risks: 1=50.0%, 2=50.0%, 3=50.0%')

  def testWithinSmallPercentOfTop(self):
    self.risks = {1: 0.05, 2: 0.0499, 3: 0.0498}
    self.assertEqual(
        risk_report.CLRiskText(42),
        'CL-Scanner risks: 1=5.0%, 2=5.0%, 3=5.0%')
