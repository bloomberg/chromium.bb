# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import unittest

# Generated files
# pylint: disable=F0401
import sample_service_mojom


class ConstantBindingsTest(unittest.TestCase):

  def testConstantGeneration(self):
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

  def testConstantOnStructGeneration(self):
    self.assertEquals(sample_service_mojom.Foo.FOOBY, "Fooby")

  def testStructImmutability(self):
    with self.assertRaises(AttributeError):
      sample_service_mojom.Foo.FOOBY = 0
    with self.assertRaises(AttributeError):
      del sample_service_mojom.Foo.FOOBY
    with self.assertRaises(AttributeError):
      sample_service_mojom.Foo.BAR = 1
