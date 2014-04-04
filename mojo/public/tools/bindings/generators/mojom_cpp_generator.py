# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source files from a mojom.Module."""

from generate import mojom
from generate import mojom_pack
from generate import mojom_generator

from generate.template_expander import UseJinja


_kind_to_cpp_type = {
  mojom.BOOL:         "bool",
  mojom.INT8:         "int8_t",
  mojom.UINT8:        "uint8_t",
  mojom.INT16:        "int16_t",
  mojom.UINT16:       "uint16_t",
  mojom.INT32:        "int32_t",
  mojom.UINT32:       "uint32_t",
  mojom.FLOAT:        "float",
  mojom.HANDLE:       "mojo::Handle",
  mojom.DCPIPE:       "mojo::DataPipeConsumerHandle",
  mojom.DPPIPE:       "mojo::DataPipeProducerHandle",
  mojom.MSGPIPE:      "mojo::MessagePipeHandle",
  mojom.SHAREDBUFFER: "mojo::SharedBufferHandle",
  mojom.INT64:        "int64_t",
  mojom.UINT64:       "uint64_t",
  mojom.DOUBLE:       "double",
}


def GetNameForKind(kind, internal = False):
  parts = []
  if kind.imported_from:
    parts.append(kind.imported_from["namespace"])
    if internal:
      parts.append("internal")
  if kind.parent_kind:
    parts.append(kind.parent_kind.name)
  parts.append(kind.name)
  return "::".join(parts)

def GetCppType(kind):
  if isinstance(kind, mojom.Struct):
    return "%s_Data*" % GetNameForKind(kind, internal=True)
  if isinstance(kind, mojom.Array):
    return "mojo::internal::Array_Data<%s>*" % GetCppType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "%sHandle" % kind.name
  if isinstance(kind, mojom.Enum):
    return "int32_t"
  if kind.spec == 's':
    return "mojo::internal::String_Data*"
  return _kind_to_cpp_type[kind]

def GetCppArrayArgWrapperType(kind):
  if isinstance(kind, (mojom.Struct, mojom.Enum)):
    return GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s >" % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "%sHandle" % kind.name
  if kind.spec == 's':
    return "mojo::String"
  return _kind_to_cpp_type[kind]

def GetCppResultWrapperType(kind):
  if isinstance(kind, (mojom.Struct, mojom.Enum)):
    return GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s >" % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "Scoped%sHandle" % kind.name
  if kind.spec == 's':
    return "mojo::String"
  if kind.spec == 'h':
    return "mojo::ScopedHandle"
  if kind.spec == 'h:d:c':
    return "mojo::ScopedDataPipeConsumerHandle"
  if kind.spec == 'h:d:p':
    return "mojo::ScopedDataPipeProducerHandle"
  if kind.spec == 'h:m':
    return "mojo::ScopedMessagePipeHandle"
  if kind.spec == 'h:s':
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetCppWrapperType(kind):
  if isinstance(kind, (mojom.Struct, mojom.Enum)):
    return GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "mojo::Array<%s >" % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "mojo::Passable<%sHandle>" % kind.name
  if kind.spec == 's':
    return "mojo::String"
  if mojom_generator.IsHandleKind(kind):
    return "mojo::Passable<%s>" % _kind_to_cpp_type[kind]
  return _kind_to_cpp_type[kind]

def GetCppConstWrapperType(kind):
  if isinstance(kind, mojom.Struct):
    return "const %s&" % GetNameForKind(kind)
  if isinstance(kind, mojom.Array):
    return "const mojo::Array<%s >&" % GetCppArrayArgWrapperType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "Scoped%sHandle" % kind.name
  if isinstance(kind, mojom.Enum):
    return GetNameForKind(kind)
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
  if kind.spec == 'h:s':
    return "mojo::ScopedSharedBufferHandle"
  if not kind in _kind_to_cpp_type:
    print "missing:", kind.spec
  return _kind_to_cpp_type[kind]

def GetCppFieldType(kind):
  if isinstance(kind, mojom.Struct):
    return ("mojo::internal::StructPointer<%s_Data>" %
        GetNameForKind(kind, internal=True))
  if isinstance(kind, mojom.Array):
    return "mojo::internal::ArrayPointer<%s>" % GetCppType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return "%sHandle" % kind.name
  if isinstance(kind, mojom.Enum):
    return GetNameForKind(kind)
  if kind.spec == 's':
    return "mojo::internal::StringPointer"
  return _kind_to_cpp_type[kind]

def IsStructWithHandles(struct):
  for pf in struct.packed.packed_fields:
    if mojom_generator.IsHandleKind(pf.field.kind):
      return True
  return False

def TranslateConstants(token, module):
  if isinstance(token, mojom.Constant):
    # Enum constants are constructed like:
    # Namespace::Struct::FIELD_NAME
    name = []
    if token.imported_from:
      name.append(token.namespace)
    if token.parent_kind:
      name.append(token.parent_kind.name)
    name.append(token.name[1])
    return "::".join(name)
  return token

def ExpressionToText(value, module):
  if value[0] != "EXPRESSION":
    raise Exception("Expected EXPRESSION, got" + value)
  return "".join(mojom_generator.ExpressionMapper(value,
      lambda token: TranslateConstants(token, module)))

_HEADER_SIZE = 8

class Generator(mojom_generator.Generator):

  cpp_filters = {
    "cpp_const_wrapper_type": GetCppConstWrapperType,
    "cpp_field_type": GetCppFieldType,
    "cpp_type": GetCppType,
    "cpp_result_type": GetCppResultWrapperType,
    "cpp_wrapper_type": GetCppWrapperType,
    "expression_to_text": ExpressionToText,
    "get_pad": mojom_pack.GetPad,
    "is_enum_kind": mojom_generator.IsEnumKind,
    "is_handle_kind": mojom_generator.IsHandleKind,
    "is_object_kind": mojom_generator.IsObjectKind,
    "is_string_kind": mojom_generator.IsStringKind,
    "is_array_kind": lambda kind: isinstance(kind, mojom.Array),
    "is_struct_with_handles": IsStructWithHandles,
    "struct_size": lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
    "struct_from_method": mojom_generator.GetStructFromMethod,
    "response_struct_from_method": mojom_generator.GetResponseStructFromMethod,
    "stylize_method": mojom_generator.StudlyCapsToCamel,
    "verify_token_type": mojom_generator.VerifyTokenType,
  }

  def GetJinjaExports(self):
    return {
      "module": self.module,
      "namespace": self.module.namespace,
      "imports": self.module.imports,
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "structs": self.GetStructs(),
      "interfaces": self.module.interfaces,
    }

  @UseJinja("cpp_templates/module.h.tmpl", filters=cpp_filters)
  def GenerateModuleHeader(self):
    return self.GetJinjaExports()

  @UseJinja("cpp_templates/module-internal.h.tmpl", filters=cpp_filters)
  def GenerateModuleInternalHeader(self):
    return self.GetJinjaExports()

  @UseJinja("cpp_templates/module.cc.tmpl", filters=cpp_filters)
  def GenerateModuleSource(self):
    return self.GetJinjaExports()

  def GenerateFiles(self):
    self.Write(self.GenerateModuleHeader(), "%s.h" % self.module.name)
    self.Write(self.GenerateModuleInternalHeader(),
        "%s-internal.h" % self.module.name)
    self.Write(self.GenerateModuleSource(), "%s.cc" % self.module.name)
