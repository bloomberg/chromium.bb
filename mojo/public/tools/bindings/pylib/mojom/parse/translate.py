# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Translates parse tree to Mojom IR."""


import ast
import re


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

def _EnumToDict(enum):
  def EnumValueToDict(enum_value):
    assert isinstance(enum_value, ast.EnumValue)
    return {'name': enum_value.name,
            'value': enum_value.value}

  assert isinstance(enum, ast.Enum)
  return {'name': enum.name,
          'fields': map(EnumValueToDict, enum.enum_value_list)}

def _ConstToDict(const):
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
              'enums': _MapTreeForType(_EnumToDict, struct.body, ast.Enum),
              'constants': _MapTreeForType(_ConstToDict, struct.body,
                                           ast.Const)}

    def InterfaceToDict(interface):
      def MethodToDict(method):
        def ParameterToDict(param):
          assert isinstance(param, ast.Parameter)
          return {'name': param.name,
                  'kind': _MapKind(param.typename),
                  'ordinal': param.ordinal.value if param.ordinal else None}

        assert isinstance(method, ast.Method)
        rv = {'name': method.name,
              'parameters': map(ParameterToDict, method.parameter_list),
              'ordinal': method.ordinal.value if method.ordinal else None}
        if method.response_parameter_list is not None:
          rv['response_parameters'] = map(ParameterToDict,
                                          method.response_parameter_list)
        return rv

      assert isinstance(interface, ast.Interface)
      attributes = _AttributeListToDict(interface.attribute_list)
      return {'name': interface.name,
              'attributes': attributes,
              'client': attributes.get('Client'),
              'methods': _MapTreeForType(MethodToDict, interface.body,
                                         ast.Method),
              'enums': _MapTreeForType(_EnumToDict, interface.body, ast.Enum),
              'constants': _MapTreeForType(_ConstToDict, interface.body,
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
        _MapTreeForType(InterfaceToDict, tree.definition_list, ast.Interface)
    self.mojom['enums'] = \
        _MapTreeForType(_EnumToDict, tree.definition_list, ast.Enum)
    self.mojom['constants'] = \
        _MapTreeForType(_ConstToDict, tree.definition_list, ast.Const)
    return self.mojom


def Translate(tree, name):
  return _MojomBuilder().Build(tree, name)
