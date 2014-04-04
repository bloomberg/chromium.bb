# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import mojom_data
import mojom_test
import sys

EXPECT_EQ = mojom_test.EXPECT_EQ
EXPECT_TRUE = mojom_test.EXPECT_TRUE
RunTest = mojom_test.RunTest


def DeepEquals(d1, d2):
  if d1 == d2:
    return True
  if d2.__class__ != d2.__class__:
    return False
  if isinstance(d1, dict):
    if set(d1.keys()) != set(d2.keys()):
      return False
    for key in d1.keys():
      if not DeepEquals(d1[key], d2[key]):
        return False
    return True
  if isinstance(d1, (list, tuple)):
    if len(d1) != len(d2):
      return False
    for i in range(len(d1)):
      if not DeepEquals(d1[i], d2[i]):
        return False
    return True
  return False


test_dict = {
  'name': 'test',
  'namespace': 'testspace',
  'structs': [{
    'name': 'teststruct',
    'fields': [
      {'name': 'testfield1', 'kind': 'i32'},
      {'name': 'testfield2', 'kind': 'a:i32', 'ordinal': 42}]}],
  'interfaces': [{
    'name': 'Server',
    'peer': None,
    'methods': [{
      'name': 'Foo',
      'parameters': [
        {'name': 'foo', 'kind': 'i32'},
        {'name': 'bar', 'kind': 'a:x:teststruct'}],
    'ordinal': 42}]}]
}


def TestRead():
  module = mojom_data.ModuleFromData(test_dict)
  return mojom_test.TestTestModule(module)


def TestWrite():
  module = mojom_test.BuildTestModule()
  d = mojom_data.ModuleToData(module)
  return EXPECT_TRUE(DeepEquals(test_dict, d))


def TestWriteRead():
  module1 = mojom_test.BuildTestModule()

  dict1 = mojom_data.ModuleToData(module1)
  module2 = mojom_data.ModuleFromData(dict1)
  return EXPECT_TRUE(mojom_test.ModulesAreEqual(module1, module2))


def Main(args):
  errors = 0
  errors += RunTest(TestWriteRead)
  errors += RunTest(TestRead)
  errors += RunTest(TestWrite)

  return errors


if __name__ == '__main__':
  sys.exit(Main(sys.argv[1:]))
