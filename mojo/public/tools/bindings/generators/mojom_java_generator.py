# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates java source files from a mojom.Module."""

import argparse
import os
import re

from jinja2 import contextfilter

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja


GENERATOR_PREFIX = 'java'

_spec_to_java_type = {
  'b':     'boolean',
  'd':     'double',
  'f':     'float',
  'h:d:c': 'org.chromium.mojo.system.DataPipe.ConsumerHandle',
  'h:d:p': 'org.chromium.mojo.system.DataPipe.ProducerHandle',
  'h:m':   'org.chromium.mojo.system.MessagePipeHandle',
  'h':     'org.chromium.mojo.system.UntypedHandle',
  'h:s':   'org.chromium.mojo.system.SharedBufferHandle',
  'i16':   'short',
  'i32':   'int',
  'i64':   'long',
  'i8':    'byte',
  's':     'String',
  'u16':   'short',
  'u32':   'int',
  'u64':   'long',
  'u8':    'byte',
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
  if isinstance(element, (mojom.Enum,
                          mojom.Interface,
                          mojom.Struct)):
    return UpperCamelCase(element.name)
  if isinstance(element, mojom.InterfaceRequest):
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

def ParseStringAttribute(attribute):
  assert isinstance(attribute, basestring)
  return attribute

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

@contextfilter
def GetJavaType(context, kind):
  if isinstance(kind, (mojom.Struct, mojom.Interface)):
    return GetNameForKind(context, kind)
  if isinstance(kind, mojom.InterfaceRequest):
    return GetNameForKind(context, kind.kind)
  if isinstance(kind, (mojom.Array, mojom.FixedArray)):
    return "%s[]" % GetJavaType(context, kind.kind)
  if isinstance(kind, mojom.Enum):
    return "int"
  return _spec_to_java_type[kind.spec]

def IsHandle(kind):
  return kind.spec[0] == 'h'

@contextfilter
def DefaultValue(context, field):
  assert field.default
  if isinstance(field.kind, mojom.Struct):
    assert field.default == "default"
    return "new %s()" % GetJavaType(context, field.kind)
  return "(%s) %s" % (GetJavaType(context, field.kind),
                      ExpressionToText(context, field.default))

@contextfilter
def ExpressionToText(context, token):
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
  # Add Long suffix to all number literals.
  if re.match('^[0-9]+$', token):
    return token + 'L'
  return token

def GetConstantsMainEntityName(module):
  if 'JavaConstantsClassName' in module.attributes:
    return ParseStringAttribute(module.attributes['JavaConstantsClassName'])
  # This constructs the name of the embedding classes for module level constants
  # by extracting the mojom's filename and prepending it to Constants.
  return (UpperCamelCase(module.path.split('/')[-1].rsplit('.', 1)[0]) +
          'Constants')

class Generator(generator.Generator):

  java_filters = {
    "default_value": DefaultValue,
    "expression_to_text": ExpressionToText,
    "is_handle": IsHandle,
    "java_type": GetJavaType,
    "name": GetNameForElement,
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
