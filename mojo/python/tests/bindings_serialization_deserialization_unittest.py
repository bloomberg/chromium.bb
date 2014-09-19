# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import math
import unittest

# pylint: disable=F0401
import mojo.bindings.reflection as reflection
import mojo.system

# Generated files
# pylint: disable=F0401
import sample_import_mojom
import sample_import2_mojom
import sample_service_mojom


def _NewHandle():
  return mojo.system.MessagePipe().handle0


def _TestEquality(x, y):
  if x == y:
    return True

  if type(x) != type(y):
    print '\n%r != %r. Element are not of the same type.' % (x, y)
    return False

  if isinstance(x, float) and math.isnan(x) and math.isnan(y):
    return True

  if hasattr(x, '__len__'):
    if len(x) != len(y):
      print '\n%r != %r. Iterables are not of the same size.' % (x, y)
      return False
    for (x1, y1) in zip(x, y):
      if not _TestEquality(x1, y1):
        return False
    return True

  if (hasattr(x, '__metaclass__') and
      x.__metaclass__ == reflection.MojoStructType):
    properties = [p for p in dir(x) if not p.startswith('_')]
    for p in properties:
      p1 = getattr(x, p)
      p2 = getattr(y, p)
      if not hasattr(p1, '__call__') and not _TestEquality(p1, p2):
        print '\n%r != %r. Not equal for property %r.' % (x, y, p)
        return False
    return True

  return False


def _NewBar():
  bar_instance = sample_service_mojom.Bar()
  bar_instance.alpha = 22
  bar_instance.beta = 87
  bar_instance.gamma = 122
  bar_instance.type = sample_service_mojom.Bar.Type.BOTH
  return bar_instance


def _NewFoo():
  foo_instance = sample_service_mojom.Foo()
  foo_instance.name = "Foo.name"
  foo_instance.x = 23
  foo_instance.y = -23
  foo_instance.a = False
  foo_instance.b = True
  foo_instance.c = True
  foo_instance.bar = _NewBar()
  foo_instance.extra_bars = [
      _NewBar(),
      _NewBar(),
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
  foo_instance.array_of_bools = [ True, 0, 1, 2, 0, 0, 0, 0, 0, True ]
  return foo_instance


class SerializationDeserializationTest(unittest.TestCase):

  def testTestEquality(self):
    self.assertFalse(_TestEquality(1, 2))

  def testFooSerialization(self):
    (data, _) = _NewFoo().Serialize()
    self.assertTrue(len(data))
    self.assertEquals(len(data) % 8, 0)

  def testFooDeserialization(self):
    (data, handles) = _NewFoo().Serialize()
    self.assertTrue(
        sample_service_mojom.Foo.Deserialize(data, handles))

  def testFooSerializationDeserialization(self):
    foo1 = _NewFoo()
    (data, handles) = foo1.Serialize()
    foo2 = sample_service_mojom.Foo.Deserialize(data, handles)
    self.assertTrue(_TestEquality(foo1, foo2))

  def testDefaultsTestSerializationDeserialization(self):
    v1 = sample_service_mojom.DefaultsTest()
    v1.a18 = []
    v1.a19 = ""
    v1.a21 = sample_import_mojom.Point()
    v1.a22.location = sample_import_mojom.Point()
    v1.a22.size = sample_import2_mojom.Size()
    (data, handles) = v1.Serialize()
    v2 = sample_service_mojom.DefaultsTest.Deserialize(data, handles)
    self.assertTrue(_TestEquality(v1, v2))

  def testFooDeserializationError(self):
    with self.assertRaises(Exception):
      sample_service_mojom.Foo.Deserialize("", [])
