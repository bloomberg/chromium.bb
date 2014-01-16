# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source files from a mojom.Module."""

import mojom
import mojom_pack
import mojom_generator

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
  mojom.DCPIPE:  "mojo::DataPipeConsumerHandle",
  mojom.DPPIPE:  "mojo::DataPipeProducerHandle",
  mojom.MSGPIPE: "mojo::MessagePipeHandle",
  mojom.INT64:   "int64_t",
  mojom.UINT64:  "uint64_t",
  mojom.DOUBLE:  "double",
}


def GetCppType(kind):
  if isinstance(kind, mojom.Struct):
    return "%s_Data*" % kind.name
  if isinstance(kind, mojom.Array):
    return "mojo::internal::Array_Data<%s>*" % GetCppType(kind.kind)
  if kind.spec == 's':
    return "mojo::internal::String_Data*"
  return _kind_to_cpp_type[kind]

def GetCppArrayArgWrapperType(kind):
  if isinstance(kind, mojom.Struct):
    return kind.name
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s >" % GetCppArrayArgWrapperType(kind.kind)
  if kind.spec == 's':
    return "mojo::String"
  return _kind_to_cpp_type[kind]

def GetCppWrapperType(kind):
  if isinstance(kind, mojom.Struct):
    return kind.name
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s >" % GetCppArrayArgWrapperType(kind.kind)
  if kind.spec == 's':
    return "mojo::String"
  if mojom_generator.IsHandleKind(kind):
    return "mojo::Passable<%s>" % _kind_to_cpp_type[kind]
  return _kind_to_cpp_type[kind]

def GetCppConstWrapperType(kind):
  if isinstance(kind, mojom.Struct):
    return "const %s&" % kind.name
  if isinstance(kind, mojom.Array):
    return "const mojo::Array<%s >&" % GetCppArrayArgWrapperType(kind.kind)
  if kind.spec == 's':
    return "const mojo::String&"
  if kind.spec == 'h':
    return "mojo::ScopedHandle"
  if kind.spec == 'h:d:c':
    return "mojo::ScopedDataPipeConsumerHandle"
  if kind.spec == 'h:d:p':
    return "mojo::ScopedDataPipeProducerHandle"
  if kind.spec == 'h:m':
    return "mojo::ScopedMessagePipeHandle"
  return _kind_to_cpp_type[kind]

def GetCppFieldType(kind):
  if mojom_generator.IsHandleKind(kind):
    return _kind_to_cpp_type[kind]
  if isinstance(kind, mojom.Struct):
    return "mojo::internal::StructPointer<%s_Data>" % kind.name
  if isinstance(kind, mojom.Array):
    return "mojo::internal::ArrayPointer<%s>" % GetCppType(kind.kind)
  if kind.spec == 's':
    return "mojo::internal::StringPointer"
  return _kind_to_cpp_type[kind]

def IsStructWithHandles(struct):
  for pf in struct.packed.packed_fields:
    if mojom_generator.IsHandleKind(pf.field.kind):
      return True
  return False

_HEADER_SIZE = 8

class CppGenerator(mojom_generator.Generator):

  cpp_filters = {
    "camel_to_underscores": mojom_generator.CamelToUnderscores,
    "cpp_const_wrapper_type": GetCppConstWrapperType,
    "cpp_field_type": GetCppFieldType,
    "cpp_type": GetCppType,
    "cpp_wrapper_type": GetCppWrapperType,
    "get_pad": mojom_pack.GetPad,
    "is_handle_kind": mojom_generator.IsHandleKind,
    "is_object_kind": mojom_generator.IsObjectKind,
    "is_struct_with_handles": IsStructWithHandles,
    "struct_size": lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
    "struct_from_method": mojom_generator.GetStructFromMethod,
    "stylize_method": mojom_generator.StudlyCapsToCamel,
  }

  @UseJinja("cpp_templates/module.h.tmpl", filters=cpp_filters)
  def GenerateModuleHeader(self):
    return {
      "module_name": self.module.name,
      "namespace": self.module.namespace,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  @UseJinja("cpp_templates/module_internal.h.tmpl", filters=cpp_filters)
  def GenerateModuleInternalHeader(self):
    return {
      "module_name": self.module.name,
      "namespace": self.module.namespace,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  @UseJinja("cpp_templates/module.cc.tmpl", filters=cpp_filters)
  def GenerateModuleSource(self):
    return {
      "module_name": self.module.name,
      "namespace": self.module.namespace,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  def GenerateFiles(self):
    self.Write(self.GenerateModuleHeader(), "%s.h" % self.module.name)
    self.Write(self.GenerateModuleInternalHeader(),
        "%s_internal.h" % self.module.name)
    self.Write(self.GenerateModuleSource(), "%s.cc" % self.module.name)
