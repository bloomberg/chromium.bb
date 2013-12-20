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


_kind_to_javascript_default_value = {
  mojom.BOOL:    "false",
  mojom.INT8:    "0",
  mojom.UINT8:   "0",
  mojom.INT16:   "0",
  mojom.UINT16:  "0",
  mojom.INT32:   "0",
  mojom.UINT32:  "0",
  mojom.FLOAT:   "0",
  mojom.HANDLE:  "core.kInvalidHandle",
  mojom.DCPIPE:  "core.kInvalidHandle",
  mojom.DPPIPE:  "core.kInvalidHandle",
  mojom.MSGPIPE: "core.kInvalidHandle",
  mojom.INT64:   "0",
  mojom.UINT64:  "0",
  mojom.DOUBLE:  "0",
  mojom.STRING:  '""',
}


def JavaScriptDefaultValue(field):
  if field.default:
    return field.default
  if field.kind in mojom.PRIMITIVES:
    return _kind_to_javascript_default_value[field.kind]
  if isinstance(field.kind, mojom.Struct):
    return "null";
  if isinstance(field.kind, mojom.Array):
    return "[]";


def JavaScriptPayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0;
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = mojom_pack.GetPad(offset, 8)
  return offset + pad;


_kind_to_javascript_type = {
  mojom.BOOL:    "codec.Uint8",
  mojom.INT8:    "codec.Int8",
  mojom.UINT8:   "codec.Uint8",
  mojom.INT16:   "codec.Int16",
  mojom.UINT16:  "codec.Uint16",
  mojom.INT32:   "codec.Int32",
  mojom.UINT32:  "codec.Uint32",
  mojom.FLOAT:   "codec.Float",
  mojom.HANDLE:  "codec.Handle",
  mojom.DCPIPE:  "codec.Handle",
  mojom.DPPIPE:  "codec.Handle",
  mojom.MSGPIPE: "codec.Handle",
  mojom.INT64:   "codec.Int64",
  mojom.UINT64:  "codec.Uint64",
  mojom.DOUBLE:  "codec.Double",
  mojom.STRING:  "codec.String",
}


def GetJavaScriptType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_javascript_type[kind]
  if isinstance(kind, mojom.Struct):
    return "new codec.PointerTo(%s)" % GetJavaScriptType(kind.name)
  if isinstance(kind, mojom.Array):
    return "new codec.ArrayOf(%s)" % GetJavaScriptType(kind.kind)
  return kind


_kind_to_javascript_decode_snippet = {
  mojom.BOOL:    "read8() & 1",
  mojom.INT8:    "read8()",
  mojom.UINT8:   "read8()",
  mojom.INT16:   "read16()",
  mojom.UINT16:  "read16()",
  mojom.INT32:   "read32()",
  mojom.UINT32:  "read32()",
  mojom.FLOAT:   "decodeFloat()",
  mojom.HANDLE:  "decodeHandle()",
  mojom.DCPIPE:  "decodeHandle()",
  mojom.DPPIPE:  "decodeHandle()",
  mojom.MSGPIPE: "decodeHandle()",
  mojom.INT64:   "read64()",
  mojom.UINT64:  "read64()",
  mojom.DOUBLE:  "decodeDouble()",
  mojom.STRING:  "decodeStringPointer()",
}


def JavaScriptDecodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_javascript_decode_snippet[kind]
  if isinstance(kind, mojom.Struct):
    return "decodeStructPointer(%s)" % GetJavaScriptType(kind.name);
  if isinstance(kind, mojom.Array):
    return "decodeArrayPointer(%s)" % GetJavaScriptType(kind.kind);


_kind_to_javascript_encode_snippet = {
  mojom.BOOL:    "write8(1 & ",
  mojom.INT8:    "write8(",
  mojom.UINT8:   "write8(",
  mojom.INT16:   "write16(",
  mojom.UINT16:  "write16(",
  mojom.INT32:   "write32(",
  mojom.UINT32:  "write32(",
  mojom.FLOAT:   "encodeFloat(",
  mojom.HANDLE:  "encodeHandle(",
  mojom.DCPIPE:  "encodeHandle(",
  mojom.DPPIPE:  "encodeHandle(",
  mojom.MSGPIPE: "encodeHandle(",
  mojom.INT64:   "write64(",
  mojom.UINT64:  "write64(",
  mojom.DOUBLE:  "encodeDouble(",
  mojom.STRING:  "encodeStringPointer(",
}


def JavaScriptEncodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_javascript_encode_snippet[kind]
  if isinstance(kind, mojom.Struct):
    return "encodeStructPointer(%s, " % GetJavaScriptType(kind.name);
  if isinstance(kind, mojom.Array):
    return "encodeArrayPointer(%s, " % GetJavaScriptType(kind.kind);

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

def GetStructInfo(exported, struct):
  struct.packed = mojom_pack.PackedStruct(struct)
  struct.exported = exported
  struct.bytes = mojom_pack.GetByteLayout(struct.packed)
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
    "struct_from_method": GetStructFromMethod,
    "stylize_method": mojom_generator.StudlyCapsToCamel,
  }

  js_filters = {
    "default_value": JavaScriptDefaultValue,
    "payload_size": JavaScriptPayloadSize,
    "decode_snippet": JavaScriptDecodeSnippet,
    "encode_snippet": JavaScriptEncodeSnippet,
    "stylize_method": mojom_generator.StudlyCapsToCamel,
  }

  def GetStructsFromMethods(self):
    result = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        result.append(mojom_generator.GetStructFromMethod(interface, method))
    return map(partial(GetStructInfo, False), result)

  def GetStructs(self):
    return map(partial(GetStructInfo, True), self.module.structs)

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

  @UseJinja("js_templates/module.js.tmpl", filters=js_filters)
  def GenerateJsModule(self):
    return {
      "enums": self.module.enums,
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "interfaces": self.module.interfaces,
    }

  def Write(self, contents, filename):
    if self.output_dir is None:
      print contents
      return
    with open(os.path.join(self.output_dir, filename), "w+") as f:
      f.write(contents)

  def GenerateFiles(self):
    self.Write(self.GenerateModuleHeader(), "%s.h" % self.module.name)
    self.Write(self.GenerateModuleInternalHeader(),
        "%s_internal.h" % self.module.name)
    self.Write(self.GenerateModuleSource(), "%s.cc" % self.module.name)
    self.Write(self.GenerateJsModule(), "%s.js" % self.module.name)
