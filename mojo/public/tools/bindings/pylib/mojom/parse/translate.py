# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Translates parse tree to Mojom IR."""


import ast
import re


def _MapTreeForName(func, tree, name):
  assert isinstance(name, str)
  if not tree:
    return []
  return [func(subtree) for subtree in tree \
              if isinstance(subtree, tuple) and subtree[0] == name]

def _MapTreeForType(func, tree, type_to_map):
  assert isinstance(type_to_map, type)
  if not tree:
    return []
  return [func(subtree) for subtree in tree if isinstance(subtree, type_to_map)]

_FIXED_ARRAY_REGEXP = re.compile(r'\[[0-9]+\]')

def _MapKind(kind):
  map_to_kind = {'bool': 'b',
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
    typename = kind[0:-2]
    if _FIXED_ARRAY_REGEXP.search(typename):
      raise Exception("Arrays of fixed sized arrays not supported")
    return 'a:' + _MapKind(typename)
  if kind.endswith(']'):
    lbracket = kind.rfind('[')
    typename = kind[0:lbracket]
    if typename.find('[') != -1:
      raise Exception("Fixed sized arrays of arrays not supported")
    return 'a' + kind[lbracket+1:-1] + ':' + _MapKind(typename)
  if kind.endswith('&'):
    return 'r:' + _MapKind(kind[0:-1])
  if kind in map_to_kind:
    return map_to_kind[kind]
  return 'x:' + kind

def _AttributeListToDict(attribute_list):
  if attribute_list is None:
    return {}
  assert isinstance(attribute_list, ast.AttributeList)
  # TODO(vtl): Check for duplicate keys here.
  return dict([(attribute.key, attribute.value)
                   for attribute in attribute_list])

def _MapMethod(tree):
  assert isinstance(tree[2], ast.ParameterList)
  assert tree[3] is None or isinstance(tree[3], ast.Ordinal)
  assert tree[4] is None or isinstance(tree[2], ast.ParameterList)

  def ParameterToDict(param):
    assert isinstance(param, ast.Parameter)
    return {'name': param.name,
            'kind': _MapKind(param.typename),
            'ordinal': param.ordinal.value if param.ordinal else None}

  method = {'name': tree[1],
            'parameters': map(ParameterToDict, tree[2]),
            'ordinal': tree[3].value if tree[3] else None}
  if tree[4]:
    method['response_parameters'] = map(ParameterToDict, tree[4])
  return method

def _MapInterface(tree):
  interface = {}
  interface['name'] = tree[1]
  interface['attributes'] = _AttributeListToDict(tree[2])
  interface['client'] = interface['attributes'].get('Client')
  interface['methods'] = _MapTreeForName(_MapMethod, tree[3], 'METHOD')
  interface['enums'] = _MapTreeForType(_MapEnum, tree[3], ast.Enum)
  interface['constants'] = _MapTreeForType(_MapConstant, tree[3], ast.Const)
  return interface

def _MapEnum(enum):
  def EnumValueToDict(enum_value):
    assert isinstance(enum_value, ast.EnumValue)
    return {'name': enum_value.name,
            'value': enum_value.value}

  assert isinstance(enum, ast.Enum)
  return {'name': enum.name,
          'fields': map(EnumValueToDict, enum.enum_value_list)}

def _MapConstant(const):
  assert isinstance(const, ast.Const)
  return {'name': const.name,
          'kind': _MapKind(const.typename),
          'value': const.value}


class _MojomBuilder(object):
  def __init__(self):
    self.mojom = {}

  def Build(self, tree, name):
    def StructToDict(struct):
      def StructFieldToDict(struct_field):
        assert isinstance(struct_field, ast.StructField)
        return {'name': struct_field.name,
                'kind': _MapKind(struct_field.typename),
                'ordinal': struct_field.ordinal.value \
                    if struct_field.ordinal else None,
                'default': struct_field.default_value}

      assert isinstance(struct, ast.Struct)
      return {'name': struct.name,
              'attributes': _AttributeListToDict(struct.attribute_list),
              'fields': _MapTreeForType(StructFieldToDict, struct.body,
                                        ast.StructField),
              'enums': _MapTreeForType(_MapEnum, struct.body, ast.Enum),
              'constants': _MapTreeForType(_MapConstant, struct.body,
                                           ast.Const)}

    assert isinstance(tree, ast.Mojom)
    self.mojom['name'] = name
    self.mojom['namespace'] = tree.module.name[1] if tree.module else ''
    self.mojom['imports'] = \
        [{'filename': imp.import_filename} for imp in tree.import_list]
    self.mojom['attributes'] = \
        _AttributeListToDict(tree.module.attribute_list) if tree.module else {}
    self.mojom['structs'] = \
        _MapTreeForType(StructToDict, tree.definition_list, ast.Struct)
    self.mojom['interfaces'] = \
        _MapTreeForName(_MapInterface, tree.definition_list, 'INTERFACE')
    self.mojom['enums'] = \
        _MapTreeForType(_MapEnum, tree.definition_list, ast.Enum)
    self.mojom['constants'] = \
        _MapTreeForType(_MapConstant, tree.definition_list, ast.Const)
    return self.mojom


def Translate(tree, name):
  return _MojomBuilder().Build(tree, name)
