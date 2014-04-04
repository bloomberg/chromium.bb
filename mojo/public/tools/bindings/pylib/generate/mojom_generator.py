# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code shared by the various language-specific code generators."""

import os
import mojom
import mojom_pack
import re
from functools import partial

def GetStructFromMethod(interface, method):
  """Converts a method's parameters into the fields of a struct."""
  params_class = "%s_%s_Params" % (interface.name, method.name)
  struct = mojom.Struct(params_class)
  for param in method.parameters:
    struct.AddField(param.name, param.kind, param.ordinal)
  struct.packed = mojom_pack.PackedStruct(struct)
  return struct

def GetResponseStructFromMethod(interface, method):
  """Converts a method's response_parameters into the fields of a struct."""
  params_class = "%s_%s_ResponseParams" % (interface.name, method.name)
  struct = mojom.Struct(params_class)
  for param in method.response_parameters:
    struct.AddField(param.name, param.kind, param.ordinal)
  struct.packed = mojom_pack.PackedStruct(struct)
  return struct

def GetStructInfo(exported, struct):
  struct.packed = mojom_pack.PackedStruct(struct)
  struct.bytes = mojom_pack.GetByteLayout(struct.packed)
  struct.exported = exported
  return struct

def IsStringKind(kind):
  return kind.spec == 's'

def IsEnumKind(kind):
  return isinstance(kind, mojom.Enum)

def IsObjectKind(kind):
  return isinstance(kind, (mojom.Struct, mojom.Array)) or IsStringKind(kind)

def IsHandleKind(kind):
  return kind.spec.startswith('h') or isinstance(kind, mojom.Interface)

def StudlyCapsToCamel(studly):
  return studly[0].lower() + studly[1:]

def VerifyTokenType(token, expected):
  """Used to check that arrays and objects are used correctly as default
  values. Arrays are tokens that look like ('ARRAY', element0, element1...).
  See mojom_parser.py for their representation.
  """
  if not isinstance(token, tuple):
    raise Exception("Expected token type '%s'. Invalid token '%s'." %
        (expected, token))
  if token[0] != expected:
    raise Exception("Expected token type '%s'. Got '%s'." %
        (expected, token[0]))

def ExpressionMapper(expression, mapper):
  if isinstance(expression, tuple) and expression[0] == 'EXPRESSION':
    result = []
    for each in expression[1]:
      result.extend(ExpressionMapper(each, mapper))
    return result
  return [mapper(expression)]

class Generator(object):
  # Pass |output_dir| to emit files to disk. Omit |output_dir| to echo all
  # files to stdout.
  def __init__(self, module, output_dir=None):
    self.module = module
    self.output_dir = output_dir

  def GetStructsFromMethods(self):
    result = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        result.append(GetStructFromMethod(interface, method))
        if method.response_parameters != None:
          result.append(GetResponseStructFromMethod(interface, method))
    return map(partial(GetStructInfo, False), result)

  def GetStructs(self):
    return map(partial(GetStructInfo, True), self.module.structs)

  def Write(self, contents, filename):
    if self.output_dir is None:
      print contents
      return
    with open(os.path.join(self.output_dir, filename), "w+") as f:
      f.write(contents)

  def GenerateFiles(self):
    raise NotImplementedError("Subclasses must override/implement this method")
