# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates source files from a mojom.Module."""

import os
import mojom
import mojom_pack
import mojom_generator

from functools import partial
from template_expander import UseJinja


_kind_to_cpp_type = {
  mojom.BOOL:    "bool",
  mojom.INT8:    "int8_t",
  mojom.UINT8:   "uint8_t",
  mojom.INT16:   "int16_t",
  mojom.UINT16:  "uint16_t",
  mojom.INT32:   "int32_t",
  mojom.UINT32:  "uint32_t",
  mojom.FLOAT:   "float",
  mojom.HANDLE:  "mojo::Handle",
  mojom.MSGPIPE: "mojo::MessagePipeHandle",
  mojom.INT64:   "int64_t",
  mojom.UINT64:  "uint64_t",
  mojom.DOUBLE:  "double",
}

def GetType(kind):
  if isinstance(kind, mojom.Struct):
    return "%s_Data*" % kind.name
  if isinstance(kind, mojom.Array):
    return "mojo::internal::Array_Data<%s>*" % GetType(kind.kind)
  if kind.spec == 's':
    return "mojo::internal::String_Data*"
  return _kind_to_cpp_type[kind]

def GetWrapperType(kind):
  if isinstance(kind, mojom.Struct):
    return "%s" % kind.name
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s >" % GetWrapperType(kind.kind)
  if kind.spec == 's':
    return "mojo::String"
  if kind.spec == 'h':
    return "mojo::Passable<mojo::Handle>"
  if kind.spec == 'h:m':
    return "mojo::Passable<mojo::MessagePipeHandle>"
  return _kind_to_cpp_type[kind]

def GetConstWrapperType(kind):
  if isinstance(kind, mojom.Struct):
    return "const %s&" % kind.name
  if isinstance(kind, mojom.Array):
    return "const mojo::Array<%s >&" % GetWrapperType(kind.kind)
  if kind.spec == 's':
    return "const mojo::String&"
  if kind.spec == 'h':
    return "mojo::ScopedHandle"
  if kind.spec == 'h:m':
    return "mojo::ScopedMessagePipeHandle"
  return _kind_to_cpp_type[kind]

def GetFieldType(kind):
  if mojom_generator.IsHandleKind(kind):
    return _kind_to_cpp_type[kind]
  if isinstance(kind, mojom.Struct):
    return "mojo::internal::StructPointer<%s_Data>" % kind.name
  if isinstance(kind, mojom.Array):
    return "mojo::internal::ArrayPointer<%s>" % GetType(kind.kind)
  if kind.spec == 's':
    return "mojo::internal::StringPointer"
  return _kind_to_cpp_type[kind]

def GetStructInfo(exported, struct):
  struct.packed = mojom_pack.PackedStruct(struct)
  struct.exported = exported
  return struct

def GetStructFromMethod(interface, method):
  struct = mojom_generator.GetStructFromMethod(interface, method)
  struct.packed = mojom_pack.PackedStruct(struct)
  return struct

def IsStructWithHandles(struct):
  for pf in struct.packed.packed_fields:
    if mojom_generator.IsHandleKind(pf.field.kind):
      return True
  return False

_HEADER_SIZE = 8

class JinjaGenerator(mojom_generator.Generator):

  filters = {
    "camel_to_underscores": mojom_generator.CamelToUnderscores,
    "cpp_const_wrapper_type": GetConstWrapperType,
    "cpp_field_type": GetFieldType,
    "cpp_type": GetType,
    "cpp_wrapper_type": GetWrapperType,
    "get_pad": mojom_pack.GetPad,
    "is_handle_kind": mojom_generator.IsHandleKind,
    "is_object_kind": mojom_generator.IsObjectKind,
    "is_struct_with_handles": IsStructWithHandles,
    "struct_size": lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
    "struct_from_method": GetStructFromMethod,
    "stylize_method": mojom_generator.StudlyCapsToCamel,
  }

  def GetStructs(self):
    return map(partial(GetStructInfo, True), self.module.structs)

  @UseJinja("cpp_templates/module.h.tmpl", filters=filters)
  def GenerateModuleHeader(self):
    return {
      "module_name": self.module.name,
      "namespace": self.module.namespace,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  @UseJinja("cpp_templates/module_internal.h.tmpl", filters=filters)
  def GenerateModuleInternalHeader(self):
    return {
      "module_name": self.module.name,
      "namespace": self.module.namespace,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  @UseJinja("cpp_templates/module.cc.tmpl", filters=filters)
  def GenerateModuleSource(self):
    return {
      "module_name": self.module.name,
      "namespace": self.module.namespace,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  def Write(self, contents, filename):
    if self.output_dir is None:
      print contents
      return
    with open(os.path.join(self.output_dir, filename), "w+") as f:
      f.write(contents)

  def GenerateFiles(self):
    self.Write(self.GenerateModuleHeader(), "%s.j.h" % self.module.name)
    self.Write(self.GenerateModuleInternalHeader(),
        "%s_internal.j.h" % self.module.name)
    self.Write(self.GenerateModuleSource(), "%s.j.cc" % self.module.name)
