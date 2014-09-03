# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import unittest

# Generated files
# pylint: disable=F0401
import sample_service_mojom


class ConstantBindingsTest(unittest.TestCase):

  def test_constant_generation(self):
    self.assertEquals(sample_service_mojom.TWELVE, 12)
    self.assertEquals(sample_service_mojom.TOO_BIG_FOR_SIGNED_INT64,
                      9999999999999999999)
    self.assertEquals(sample_service_mojom.DOUBLE_INFINITY,
                      float('inf'))
    self.assertEquals(sample_service_mojom.DOUBLE_NEGATIVE_INFINITY,
                      float('-inf'))
    self.assertTrue(math.isnan(sample_service_mojom.DOUBLE_NA_N))
    self.assertEquals(sample_service_mojom.FLOAT_INFINITY,
                      float('inf'))
    self.assertEquals(sample_service_mojom.FLOAT_NEGATIVE_INFINITY,
                      float('-inf'))
    self.assertTrue(math.isnan(sample_service_mojom.FLOAT_NA_N))
