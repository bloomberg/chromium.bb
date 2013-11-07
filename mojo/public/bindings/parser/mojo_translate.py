#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Translate parse tree to Mojom IR"""


import sys


def MapKind(kind):
  map_to_kind = { 'bool': 'b',
                  'int8': 'i8',
                  'int16': 'i16',
                  'int32': 'i32',
                  'int64': 'i64',
                  'uint8': 'u8',
                  'uint16': 'u16',
                  'uint32': 'u32',
                  'uint64': 'u64',
                  'float': 'f',
                  'double': 'd',
                  'string': 's',
                  'handle': 'h' }
  if kind.endswith('[]'):
    return 'a:' + MapKind(kind[0:len(kind)-2])
  if kind in map_to_kind:
    return map_to_kind[kind]
  return 'x:' + kind


def MapOrdinal(ordinal):
  return int(ordinal[1:])  # Strip leading '@'


def MapFields(fields):
  out = []
  for field in fields:
    if field[0] == 'FIELD':
      out.append({'name': field[2],
                  'kind': MapKind(field[1]),
                  'ordinal': MapOrdinal(field[3])})
  return out


def MapParameters(parameters):
  out = []
  for parameter in parameters:
    if parameter[0] == 'PARAM':
      out.append({'name': parameter[2],
                  'kind': MapKind(parameter[1]),
                  'ordinal': MapOrdinal(parameter[3])})
  return out


def MapMethods(methods):
  out = []
  for method in methods:
    if method[0] == 'METHOD':
      out.append({'name': method[1],
                  'parameters': MapParameters(method[2]),
                  'ordinal': MapOrdinal(method[3])})
  return out


class MojomBuilder():

  def __init__(self):
    self.mojom = {}

  def AddStruct(self, name, attributes, fields):
    struct = {}
    struct['name'] = name
    struct['fields'] = MapFields(fields)
    self.mojom['structs'].append(struct)
    # TODO(darin): Add support for |attributes|

  def AddInterface(self, name, attributes, methods):
    interface = {}
    interface['name'] = name
    interface['methods'] = MapMethods(methods)
    self.mojom['interfaces'].append(interface)
    # TODO(darin): Add support for |attributes|

  def AddModule(self, name, contents):
    self.mojom['name'] = name
    self.mojom['namespace'] = name
    self.mojom['structs'] = []
    self.mojom['interfaces'] = []
    for item in contents:
      if item[0] == 'STRUCT':
        self.AddStruct(name=item[1], attributes=item[2], fields=item[3])
      elif item[0] == 'INTERFACE':
        self.AddInterface(name=item[1], attributes=item[2], methods=item[3])

  def Build(self, tree):
    if tree[0] == 'MODULE':
      self.AddModule(name=tree[1], contents=tree[2])
    return self.mojom


def Translate(tree):
  return MojomBuilder().Build(tree)


def Main():
  if len(sys.argv) < 2:
    print("usage: %s filename" % (sys.argv[0]))
    sys.exit(1)
  tree = eval(open(sys.argv[1]).read())
  result = Translate(tree)
  print(result)


if __name__ == '__main__':
  Main()
