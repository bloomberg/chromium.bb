# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates java source files from a mojom.Module."""

import argparse
import ast
import os
import re

from jinja2 import contextfilter

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja


GENERATOR_PREFIX = 'java'

_HEADER_SIZE = 8

_spec_to_java_type = {
  mojom.BOOL.spec: 'boolean',
  mojom.DCPIPE.spec: 'org.chromium.mojo.system.DataPipe.ConsumerHandle',
  mojom.DOUBLE.spec: 'double',
  mojom.DPPIPE.spec: 'org.chromium.mojo.system.DataPipe.ProducerHandle',
  mojom.FLOAT.spec: 'float',
  mojom.HANDLE.spec: 'org.chromium.mojo.system.UntypedHandle',
  mojom.INT16.spec: 'short',
  mojom.INT32.spec: 'int',
  mojom.INT64.spec: 'long',
  mojom.INT8.spec: 'byte',
  mojom.MSGPIPE.spec: 'org.chromium.mojo.system.MessagePipeHandle',
  mojom.NULLABLE_DCPIPE.spec:
      'org.chromium.mojo.system.DataPipe.ConsumerHandle',
  mojom.NULLABLE_DPPIPE.spec:
      'org.chromium.mojo.system.DataPipe.ProducerHandle',
  mojom.NULLABLE_HANDLE.spec: 'org.chromium.mojo.system.UntypedHandle',
  mojom.NULLABLE_MSGPIPE.spec: 'org.chromium.mojo.system.MessagePipeHandle',
  mojom.NULLABLE_SHAREDBUFFER.spec:
      'org.chromium.mojo.system.SharedBufferHandle',
  mojom.NULLABLE_STRING.spec: 'String',
  mojom.SHAREDBUFFER.spec: 'org.chromium.mojo.system.SharedBufferHandle',
  mojom.STRING.spec: 'String',
  mojom.UINT16.spec: 'short',
  mojom.UINT32.spec: 'int',
  mojom.UINT64.spec: 'long',
  mojom.UINT8.spec: 'byte',
}

_spec_to_decode_method = {
  mojom.BOOL.spec:                  'readBoolean',
  mojom.DCPIPE.spec:                'readConsumerHandle',
  mojom.DOUBLE.spec:                'readDouble',
  mojom.DPPIPE.spec:                'readProducerHandle',
  mojom.FLOAT.spec:                 'readFloat',
  mojom.HANDLE.spec:                'readUntypedHandle',
  mojom.INT16.spec:                 'readShort',
  mojom.INT32.spec:                 'readInt',
  mojom.INT64.spec:                 'readLong',
  mojom.INT8.spec:                  'readByte',
  mojom.MSGPIPE.spec:               'readMessagePipeHandle',
  mojom.NULLABLE_DCPIPE.spec:       'readConsumerHandle',
  mojom.NULLABLE_DPPIPE.spec:       'readProducerHandle',
  mojom.NULLABLE_HANDLE.spec:       'readUntypedHandle',
  mojom.NULLABLE_MSGPIPE.spec:      'readMessagePipeHandle',
  mojom.NULLABLE_SHAREDBUFFER.spec: 'readSharedBufferHandle',
  mojom.NULLABLE_STRING.spec:       'readString',
  mojom.SHAREDBUFFER.spec:          'readSharedBufferHandle',
  mojom.STRING.spec:                'readString',
  mojom.UINT16.spec:                'readShort',
  mojom.UINT32.spec:                'readInt',
  mojom.UINT64.spec:                'readLong',
  mojom.UINT8.spec:                 'readByte',
}

_java_primitive_to_boxed_type = {
  'boolean': 'Boolean',
  'byte':    'Byte',
  'double':  'Double',
  'float':   'Float',
  'int':     'Integer',
  'long':    'Long',
  'short':   'Short',
}


def NameToComponent(name):
  # insert '_' between anything and a Title name (e.g, HTTPEntry2FooBar ->
  # HTTP_Entry2_FooBar)
  name = re.sub('([^_])([A-Z][^A-Z_]+)', r'\1_\2', name)
  # insert '_' between non upper and start of upper blocks (e.g.,
  # HTTP_Entry2_FooBar -> HTTP_Entry2_Foo_Bar)
  name = re.sub('([^A-Z_])([A-Z])', r'\1_\2', name)
  return [x.lower() for x in name.split('_')]

def CapitalizeFirst(string):
  return string[0].upper() + string[1:]

def UpperCamelCase(name):
  return ''.join([CapitalizeFirst(x) for x in NameToComponent(name)])

def CamelCase(name):
  uccc = UpperCamelCase(name)
  return uccc[0].lower() + uccc[1:]

def ConstantStyle(name):
  components = NameToComponent(name)
  if components[0] == 'k':
    components = components[1:]
  return '_'.join([x.upper() for x in components])

def GetNameForElement(element):
  if (mojom.IsEnumKind(element) or mojom.IsInterfaceKind(element) or
      mojom.IsStructKind(element)):
    return UpperCamelCase(element.name)
  if mojom.IsInterfaceRequestKind(element):
    return GetNameForElement(element.kind)
  if isinstance(element, (mojom.Method,
                          mojom.Parameter,
                          mojom.Field)):
    return CamelCase(element.name)
  if isinstance(element,  mojom.EnumValue):
    return (UpperCamelCase(element.enum_name) + '.' +
            ConstantStyle(element.name))
  if isinstance(element, (mojom.NamedValue,
                          mojom.Constant)):
    return ConstantStyle(element.name)
  raise Exception("Unexpected element: " % element)

def GetInterfaceResponseName(method):
  return UpperCamelCase(method.name + 'Response')

def ParseStringAttribute(attribute):
  assert isinstance(attribute, basestring)
  return attribute

@contextfilter
def DecodeMethod(context, kind, offset, bit):
  def _DecodeMethodName(kind):
    if mojom.IsAnyArrayKind(kind):
      return _DecodeMethodName(kind.kind) + 's'
    if mojom.IsEnumKind(kind):
      return _DecodeMethodName(mojom.INT32)
    if mojom.IsInterfaceRequestKind(kind):
      return "readInterfaceRequest"
    if mojom.IsInterfaceKind(kind):
      return "readServiceInterface"
    return _spec_to_decode_method[kind.spec]
  methodName = _DecodeMethodName(kind)
  additionalParams = ''
  if (kind == mojom.BOOL):
    additionalParams = ', %d' % bit
  if mojom.IsInterfaceKind(kind):
    additionalParams = ', %s.BUILDER' % GetJavaType(context, kind)
  if mojom.IsAnyArrayKind(kind) and mojom.IsInterfaceKind(kind.kind):
    additionalParams = ', %s.BUILDER' % GetJavaType(context, kind.kind)
  return '%s(%s%s)' % (methodName, offset, additionalParams)

@contextfilter
def EncodeMethod(context, kind, variable, offset, bit):
  additionalParams = ''
  if (kind == mojom.BOOL):
    additionalParams = ', %d' % bit
  if mojom.IsInterfaceKind(kind):
    additionalParams = ', %s.BUILDER' % GetJavaType(context, kind)
  if mojom.IsAnyArrayKind(kind) and mojom.IsInterfaceKind(kind.kind):
    additionalParams = ', %s.BUILDER' % GetJavaType(context, kind.kind)
  return 'encode(%s, %s%s)' % (variable, offset, additionalParams)

def GetPackage(module):
  if 'JavaPackage' in module.attributes:
    return ParseStringAttribute(module.attributes['JavaPackage'])
  # Default package.
  return "org.chromium.mojom." + module.namespace

def GetNameForKind(context, kind):
  def _GetNameHierachy(kind):
    hierachy = []
    if kind.parent_kind:
      hierachy = _GetNameHierachy(kind.parent_kind)
    hierachy.append(GetNameForElement(kind))
    return hierachy

  module = context.resolve('module')
  elements = []
  if GetPackage(module) != GetPackage(kind.module):
    elements += [GetPackage(kind.module)]
  elements += _GetNameHierachy(kind)
  return '.'.join(elements)

def GetBoxedJavaType(context, kind):
  unboxed_type = GetJavaType(context, kind, False)
  if unboxed_type in _java_primitive_to_boxed_type:
    return _java_primitive_to_boxed_type[unboxed_type]
  return unboxed_type

@contextfilter
def GetJavaType(context, kind, boxed=False):
  if boxed:
    return GetBoxedJavaType(context, kind)
  if mojom.IsStructKind(kind) or mojom.IsInterfaceKind(kind):
    return GetNameForKind(context, kind)
  if mojom.IsInterfaceRequestKind(kind):
    return ("org.chromium.mojo.bindings.InterfaceRequest<%s>" %
            GetNameForKind(context, kind.kind))
  if mojom.IsAnyArrayKind(kind):
    return "%s[]" % GetJavaType(context, kind.kind)
  if mojom.IsEnumKind(kind):
    return "int"
  return _spec_to_java_type[kind.spec]

@contextfilter
def DefaultValue(context, field):
  assert field.default
  if isinstance(field.kind, mojom.Struct):
    assert field.default == "default"
    return "new %s()" % GetJavaType(context, field.kind)
  return "(%s) %s" % (
      GetJavaType(context, field.kind),
      ExpressionToText(context, field.default, kind_spec=field.kind.spec))

@contextfilter
def ConstantValue(context, constant):
  return "(%s) %s" % (
      GetJavaType(context, constant.kind),
      ExpressionToText(context, constant.value, kind_spec=constant.kind.spec))

@contextfilter
def NewArray(context, kind, size):
  if mojom.IsAnyArrayKind(kind.kind):
    return NewArray(context, kind.kind, size) + '[]'
  return 'new %s[%s]' % (GetJavaType(context, kind.kind), size)

@contextfilter
def ExpressionToText(context, token, kind_spec=''):
  def _TranslateNamedValue(named_value):
    entity_name = GetNameForElement(named_value)
    if named_value.parent_kind:
      return GetJavaType(context, named_value.parent_kind) + '.' + entity_name
    # Handle the case where named_value is a module level constant:
    if not isinstance(named_value, mojom.EnumValue):
      entity_name = (GetConstantsMainEntityName(named_value.module) + '.' +
                      entity_name)
    if GetPackage(named_value.module) == GetPackage(context.resolve('module')):
      return entity_name
    return GetPackage(named_value.module) + '.' + entity_name

  if isinstance(token, mojom.NamedValue):
    return _TranslateNamedValue(token)
  if kind_spec.startswith('i') or kind_spec.startswith('u'):
    # Add Long suffix to all integer literals.
    number = ast.literal_eval(token.lstrip('+ '))
    if not isinstance(number, (int, long)):
      raise ValueError('got unexpected type %r for int literal %r' % (
          type(number), token))
    # If the literal is too large to fit a signed long, convert it to the
    # equivalent signed long.
    if number >= 2 ** 63:
      number -= 2 ** 64
    return '%dL' % number
  return token

def IsPointerArrayKind(kind):
  if not mojom.IsAnyArrayKind(kind):
    return False
  sub_kind = kind.kind
  return mojom.IsObjectKind(sub_kind)

def GetConstantsMainEntityName(module):
  if 'JavaConstantsClassName' in module.attributes:
    return ParseStringAttribute(module.attributes['JavaConstantsClassName'])
  # This constructs the name of the embedding classes for module level constants
  # by extracting the mojom's filename and prepending it to Constants.
  return (UpperCamelCase(module.path.split('/')[-1].rsplit('.', 1)[0]) +
          'Constants')

class Generator(generator.Generator):

  java_filters = {
    "interface_response_name": GetInterfaceResponseName,
    "constant_value": ConstantValue,
    "default_value": DefaultValue,
    "decode_method": DecodeMethod,
    "expression_to_text": ExpressionToText,
    "encode_method": EncodeMethod,
    "is_handle": mojom.IsNonInterfaceHandleKind,
    "is_pointer_array_kind": IsPointerArrayKind,
    "is_struct_kind": mojom.IsStructKind,
    "java_type": GetJavaType,
    "name": GetNameForElement,
    "new_array": NewArray,
    "struct_size": lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
  }

  def GetJinjaExports(self):
    return {
      "module": self.module,
      "package": GetPackage(self.module),
    }

  @UseJinja("java_templates/enum.java.tmpl", filters=java_filters)
  def GenerateEnumSource(self, enum):
    exports = self.GetJinjaExports()
    exports.update({"enum": enum})
    return exports

  @UseJinja("java_templates/struct.java.tmpl", filters=java_filters)
  def GenerateStructSource(self, struct):
    exports = self.GetJinjaExports()
    exports.update({"struct": struct})
    return exports

  @UseJinja("java_templates/interface.java.tmpl", filters=java_filters)
  def GenerateInterfaceSource(self, interface):
    exports = self.GetJinjaExports()
    exports.update({"interface": interface})
    if interface.client:
      for client in self.module.interfaces:
        if client.name == interface.client:
          exports.update({"client": client})
    return exports

  @UseJinja("java_templates/constants.java.tmpl", filters=java_filters)
  def GenerateConstantsSource(self, module):
    exports = self.GetJinjaExports()
    exports.update({"main_entity": GetConstantsMainEntityName(module),
                    "constants": module.constants})
    return exports

  def GenerateFiles(self, unparsed_args):
    parser = argparse.ArgumentParser()
    parser.add_argument("--java_output_directory", dest="java_output_directory")
    args = parser.parse_args(unparsed_args)
    if self.output_dir and args.java_output_directory:
      self.output_dir = os.path.join(args.java_output_directory,
                                     GetPackage(self.module).replace('.', '/'))
      if not os.path.exists(self.output_dir):
        try:
          os.makedirs(self.output_dir)
        except:
          # Ignore errors on directory creation.
          pass

    for enum in self.module.enums:
      self.Write(self.GenerateEnumSource(enum),
                 "%s.java" % GetNameForElement(enum))

    for struct in self.module.structs:
      self.Write(self.GenerateStructSource(struct),
                 "%s.java" % GetNameForElement(struct))

    for interface in self.module.interfaces:
      self.Write(self.GenerateInterfaceSource(interface),
                 "%s.java" % GetNameForElement(interface))

    if self.module.constants:
      self.Write(self.GenerateConstantsSource(self.module),
                 "%s.java" % GetConstantsMainEntityName(self.module))

  def GetJinjaParameters(self):
    return {
      'lstrip_blocks': True,
      'trim_blocks': True,
    }

  def GetGlobals(self):
    return {
      'module': self.module,
    }
