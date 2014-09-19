# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest

# pylint: disable=F0401
import mojo.system

# Generated files
# pylint: disable=F0401
import sample_service_mojom


def _NewHandle():
  return mojo.system.MessagePipe().handle0

def _NewFoo():
  foo_instance = sample_service_mojom.Foo()
  foo_instance.name = "Foo.name"
  foo_instance.x = 23
  foo_instance.y = -23
  foo_instance.a = False
  foo_instance.b = True
  foo_instance.c = True
  foo_instance.bar = sample_service_mojom.Bar()
  foo_instance.extra_bars = [
      sample_service_mojom.Bar(),
      sample_service_mojom.Bar()
  ]
  foo_instance.data = 'Hello world'
  foo_instance.source = _NewHandle()
  foo_instance.input_streams = [ _NewHandle() ]
  foo_instance.output_streams = [ _NewHandle(), _NewHandle() ]
  foo_instance.array_of_array_of_bools = [ [ True, False ], [] ]
  foo_instance.multi_array_of_strings = [
      [
          [ "1", "2" ],
          [],
          [ "3", "4" ],
      ],
      [],
  ]
  foo_instance.array_of_bools = [ True, 0, 1, 2, 0 ]
  return foo_instance

class SerializationDeserializationTest(unittest.TestCase):

  def testFooSerialization(self):
    (data, _) = _NewFoo().Serialize()
    self.assertTrue(len(data))
    self.assertEquals(len(data) % 8, 0)
