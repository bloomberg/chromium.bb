# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Code shared by the various language-specific code generators."""

from functools import partial
import os.path
import re

import module as mojom
import pack

def GetStructFromMethod(method):
  """Converts a method's parameters into the fields of a struct."""
  params_class = "%s_%s_Params" % (method.interface.name, method.name)
  struct = mojom.Struct(params_class, module=method.interface.module)
  for param in method.parameters:
    struct.AddField(param.name, param.kind, param.ordinal)
  struct.packed = pack.PackedStruct(struct)
  return struct

def GetResponseStructFromMethod(method):
  """Converts a method's response_parameters into the fields of a struct."""
  params_class = "%s_%s_ResponseParams" % (method.interface.name, method.name)
  struct = mojom.Struct(params_class, module=method.interface.module)
  for param in method.response_parameters:
    struct.AddField(param.name, param.kind, param.ordinal)
  struct.packed = pack.PackedStruct(struct)
  return struct

def GetDataHeader(exported, struct):
  struct.packed = pack.PackedStruct(struct)
  struct.bytes = pack.GetByteLayout(struct.packed)
  struct.exported = exported
  return struct

def ExpectedArraySize(kind):
  if mojom.IsFixedArrayKind(kind):
    return kind.length
  return 0

def StudlyCapsToCamel(studly):
  return studly[0].lower() + studly[1:]

def CamelCaseToAllCaps(camel_case):
  return '_'.join(
      word for word in re.split(r'([A-Z][^A-Z]+)', camel_case) if word).upper()

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
        result.append(GetStructFromMethod(method))
        if method.response_parameters != None:
          result.append(GetResponseStructFromMethod(method))
    return map(partial(GetDataHeader, False), result)

  def GetStructs(self):
    return map(partial(GetDataHeader, True), self.module.structs)

  def Write(self, contents, filename):
    if self.output_dir is None:
      print contents
      return
    with open(os.path.join(self.output_dir, filename), "w+") as f:
      f.write(contents)

  def GenerateFiles(self, args):
    raise NotImplementedError("Subclasses must override/implement this method")

  def GetJinjaParameters(self):
    """Returns default constructor parameters for the jinja environment."""
    return {}

  def GetGlobals(self):
    """Returns global mappings for the template generation."""
    return {}
