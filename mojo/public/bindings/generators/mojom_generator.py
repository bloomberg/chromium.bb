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

def GetStructInfo(exported, struct):
  struct.packed = mojom_pack.PackedStruct(struct)
  struct.bytes = mojom_pack.GetByteLayout(struct.packed)
  struct.exported = exported
  return struct

def IsObjectKind(kind):
  return isinstance(kind, (mojom.Struct, mojom.Array)) or kind.spec == 's'

def IsHandleKind(kind):
  return kind.spec.startswith('h')

def CamelToUnderscores(camel):
  s = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', camel)
  return re.sub('([a-z0-9])([A-Z])', r'\1_\2', s).lower()

def StudlyCapsToCamel(studly):
  return studly[0].lower() + studly[1:]

class Generator(object):
  # Pass |output_dir| to emit files to disk. Omit |output_dir| to echo all
  # files to stdout.
  def __init__(self, module, header_dir, output_dir=None):
    self.module = module
    self.header_dir = header_dir
    self.output_dir = output_dir

  def GetStructsFromMethods(self):
    result = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        result.append(GetStructFromMethod(interface, method))
    return map(partial(GetStructInfo, False), result)

  def GetStructs(self):
    return map(partial(GetStructInfo, True), self.module.structs)

  def Write(self, contents, filename):
    if self.output_dir is None:
      print contents
      return
    with open(os.path.join(self.output_dir, filename), "w+") as f:
      f.write(contents)
