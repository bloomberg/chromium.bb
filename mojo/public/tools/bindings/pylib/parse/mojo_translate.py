#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Translate parse tree to Mojom IR"""


import os
import sys


def MapTree(func, tree, name):
  if not tree:
    return []
  return [func(subtree) for subtree in tree if subtree[0] == name]

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
                  'handle': 'h',
                  'handle<data_pipe_consumer>': 'h:d:c',
                  'handle<data_pipe_producer>': 'h:d:p',
                  'handle<message_pipe>': 'h:m',
                  'handle<shared_buffer>': 'h:s'}
  if kind.endswith('[]'):
    return 'a:' + MapKind(kind[0:len(kind)-2])
  if kind in map_to_kind:
    return map_to_kind[kind]
  return 'x:' + kind

def MapOrdinal(ordinal):
  if ordinal == None:
    return None
  return int(ordinal[1:])  # Strip leading '@'

def GetAttribute(attributes, name):
  out = None
  if attributes:
    for attribute in attributes:
      if attribute[0] == 'ATTRIBUTE' and attribute[1] == name:
        out = attribute[2]
  return out

def MapField(tree):
  return {'name': tree[2],
          'kind': MapKind(tree[1]),
          'ordinal': MapOrdinal(tree[3]),
          'default': tree[4]}

def MapParameter(tree):
  return {'name': tree[2],
          'kind': MapKind(tree[1]),
          'ordinal': MapOrdinal(tree[3])}

def MapMethod(tree):
  method = {'name': tree[1],
            'parameters': MapTree(MapParameter, tree[2], 'PARAM'),
            'ordinal': MapOrdinal(tree[3])}
  if tree[4] != None:
    method['response_parameters'] = MapTree(MapParameter, tree[4], 'PARAM')
  return method

def MapEnumField(tree):
  return {'name': tree[1],
          'value': tree[2]}

def MapStruct(tree):
  struct = {}
  struct['name'] = tree[1]
  # TODO(darin): Add support for |attributes|
  #struct['attributes'] = MapAttributes(tree[2])
  struct['fields'] = MapTree(MapField, tree[3], 'FIELD')
  struct['enums'] = MapTree(MapEnum, tree[3], 'ENUM')
  return struct

def MapInterface(tree):
  interface = {}
  interface['name'] = tree[1]
  interface['peer'] = GetAttribute(tree[2], 'Peer')
  interface['methods'] = MapTree(MapMethod, tree[3], 'METHOD')
  interface['enums'] = MapTree(MapEnum, tree[3], 'ENUM')
  return interface

def MapEnum(tree):
  enum = {}
  enum['name'] = tree[1]
  enum['fields'] = MapTree(MapEnumField, tree[2], 'ENUM_FIELD')
  return enum

def MapModule(tree, name):
  mojom = {}
  mojom['name'] = name
  mojom['namespace'] = tree[1]
  mojom['structs'] = MapTree(MapStruct, tree[2], 'STRUCT')
  mojom['interfaces'] = MapTree(MapInterface, tree[2], 'INTERFACE')
  mojom['enums'] = MapTree(MapEnum, tree[2], 'ENUM')
  return mojom

def MapImport(tree):
  import_item = {}
  import_item['filename'] = tree[1]
  return import_item


class MojomBuilder():
  def __init__(self):
    self.mojom = {}

  def Build(self, tree, name):
    modules = [MapModule(item, name)
        for item in tree if item[0] == 'MODULE']
    if len(modules) != 1:
      raise Exception('A mojom file must contain exactly 1 module.')
    self.mojom = modules[0]
    self.mojom['imports'] = MapTree(MapImport, tree, 'IMPORT')
    return self.mojom


def Translate(tree, name):
  return MojomBuilder().Build(tree, name)


def Main():
  if len(sys.argv) < 2:
    print("usage: %s filename" % (sys.argv[0]))
    sys.exit(1)
  tree = eval(open(sys.argv[1]).read())
  name = os.path.splitext(os.path.basename(sys.argv[1]))[0]
  result = Translate(tree, name)
  print(result)


if __name__ == '__main__':
  Main()
