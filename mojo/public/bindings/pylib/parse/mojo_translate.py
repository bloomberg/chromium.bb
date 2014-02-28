#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Translate parse tree to Mojom IR"""


import os
import sys


class MojomBuilder():
  def __init__(self):
    self.mojom = {}

  def MapKind(self, kind):
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
                    'handle<message_pipe>': 'h:m'}
    if kind.endswith('[]'):
      return 'a:' + self.MapKind(kind[0:len(kind)-2])
    if kind in map_to_kind:
      return map_to_kind[kind]
    return 'x:' + kind


  def MapOrdinal(self, ordinal):
    if ordinal == None:
      return None;
    return int(ordinal[1:])  # Strip leading '@'


  def GetAttribute(self, attributes, name):
    out = None
    if attributes:
      for attribute in attributes:
        if attribute[0] == 'ATTRIBUTE' and attribute[1] == name:
          out = attribute[2]
    return out


  def MapFields(self, fields):
    out = []
    for field in fields:
      if field[0] == 'FIELD':
        out.append({'name': field[2],
                    'kind': self.MapKind(field[1]),
                    'ordinal': self.MapOrdinal(field[3]),
                    'default': field[4]})
    return out


  def MapParameters(self, parameters):
    out = []
    for parameter in parameters:
      if parameter[0] == 'PARAM':
        out.append({'name': parameter[2],
                    'kind': self.MapKind(parameter[1]),
                    'ordinal': self.MapOrdinal(parameter[3])})
    return out


  def MapMethods(self, methods):
    out = []
    if methods:
      for method in methods:
        if method[0] == 'METHOD':
          out.append({'name': method[1],
                      'parameters': self.MapParameters(method[2]),
                      'ordinal': self.MapOrdinal(method[3])})
    return out


  def MapEnumFields(self, fields):
    out = []
    for field in fields:
      if field[0] == 'ENUM_FIELD':
        out.append({'name': field[1],
                    'value': field[2]})
    return out


  def MapEnums(self, enums):
    out = []
    if enums:
      for enum in enums:
        if enum[0] == 'ENUM':
          out.append({'name': enum[1],
                      'fields': self.MapEnumFields(enum[2])})
    return out


  def AddStruct(self, name, attributes, body):
    struct = {}
    struct['name'] = name
    # TODO(darin): Add support for |attributes|
    #struct['attributes'] = MapAttributes(attributes)
    struct['fields'] = self.MapFields(body)
    struct['enums'] = self.MapEnums(body)
    self.mojom['structs'].append(struct)

  def AddInterface(self, name, attributes, body):
    interface = {}
    interface['name'] = name
    interface['peer'] = self.GetAttribute(attributes, 'Peer')
    interface['methods'] = self.MapMethods(body)
    interface['enums'] = self.MapEnums(body)
    self.mojom['interfaces'].append(interface)

  def AddEnum(self, name, fields):
    # TODO(mpcomplete): add support for specifying enums as types. Right now
    # we just use int32.
    enum = {}
    enum['name'] = name
    enum['fields'] = self.MapEnumFields(fields)
    self.mojom['enums'].append(enum)

  def AddModule(self, name, namespace, contents):
    self.mojom['name'] = name
    self.mojom['namespace'] = namespace
    self.mojom['structs'] = []
    self.mojom['interfaces'] = []
    self.mojom['enums'] = []
    for item in contents:
      if item[0] == 'STRUCT':
        self.AddStruct(name=item[1], attributes=item[2], body=item[3])
      elif item[0] == 'INTERFACE':
        self.AddInterface(name=item[1], attributes=item[2], body=item[3])
      elif item[0] == 'ENUM':
        self.AddEnum(name=item[1], fields=item[2])

  def AddImport(self, filename):
    import_item = {}
    import_item['filename'] = filename
    self.mojom['imports'].append(import_item)

  def Build(self, tree, name):
    self.mojom['imports'] = []
    for item in tree:
      if item[0] == 'MODULE':
        self.AddModule(name=name, namespace=item[1], contents=item[2])
      elif item[0] == 'IMPORT':
        self.AddImport(filename=item[1])
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
