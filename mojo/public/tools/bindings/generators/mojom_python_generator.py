# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates Python source files from a mojom.Module."""

import re

import mojom.generate.generator as generator
import mojom.generate.module as mojom
from mojom.generate.template_expander import UseJinja


def NameToComponent(name):
  # insert '_' between anything and a Title name (e.g, HTTPEntry2FooBar ->
  # HTTP_Entry2_FooBar)
  name = re.sub('([^_])([A-Z][^A-Z_]+)', r'\1_\2', name)
  # insert '_' between non upper and start of upper blocks (e.g.,
  # HTTP_Entry2_FooBar -> HTTP_Entry2_Foo_Bar)
  name = re.sub('([^A-Z_])([A-Z])', r'\1_\2', name)
  return [x.lower() for x in name.split('_')]

def UpperCamelCase(name):
  return ''.join([x.capitalize() for x in NameToComponent(name)])

def CamelCase(name):
  uccc = UpperCamelCase(name)
  return uccc[0].lower() + uccc[1:]

def ConstantStyle(name):
  components = NameToComponent(name)
  if components[0] == 'k':
    components = components[1:]
  return '_'.join([x.upper() for x in components])

def GetNameForElement(element):
  if isinstance(element, (mojom.NamedValue,
                          mojom.Constant)):
    return ConstantStyle(element.name)
  raise Exception('Unexpected element: ' % element)

def TranslateConstants(token):
  if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
    # Both variable and enum constants are constructed like:
    # NamespaceUid.Struct[.Enum].CONSTANT_NAME
    name = []
    if token.imported_from:
      name.append(token.imported_from['python_module'])
    if token.parent_kind:
      name.append(GetNameForElement(token.parent_kind))
    if isinstance(token, mojom.EnumValue):
      name.append(GetNameForElement(token))
    else:
      name.append(token.name)
    return '.'.join(name)

  if isinstance(token, mojom.BuiltinValue):
    if token.value == 'double.INFINITY' or token.value == 'float.INFINITY':
      return 'float(\'inf\')';
    if (token.value == 'double.NEGATIVE_INFINITY' or
        token.value == 'float.NEGATIVE_INFINITY'):
      return 'float(\'-inf\')'
    if token.value == 'double.NAN' or token.value == 'float.NAN':
      return 'float(\'nan\')';

  return token


def ExpressionToText(value):
  return TranslateConstants(value)


class Generator(generator.Generator):

  python_filters = {
    'expression_to_text': ExpressionToText,
    'name': GetNameForElement,
  }

  @UseJinja('python_templates/module.py.tmpl', filters=python_filters)
  def GeneratePythonModule(self):
    return {
      'imports': self.GetImports(),
      'module': self.module,
    }

  def GenerateFiles(self, args):
    self.Write(self.GeneratePythonModule(),
               '%s.py' % self.module.name.replace('.mojom', '_mojom'))

  def GetImports(self):
    for each in self.module.imports:
      each['python_module'] = each['module_name'].replace('.mojom', '_mojom')
    return self.module.imports

  def GetJinjaParameters(self):
    return {
      'lstrip_blocks': True,
      'trim_blocks': True,
    }
