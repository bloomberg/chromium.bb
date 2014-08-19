# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates C++ source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja


_kind_to_cpp_type = {
  mojom.BOOL:                  "bool",
  mojom.INT8:                  "int8_t",
  mojom.UINT8:                 "uint8_t",
  mojom.INT16:                 "int16_t",
  mojom.UINT16:                "uint16_t",
  mojom.INT32:                 "int32_t",
  mojom.UINT32:                "uint32_t",
  mojom.FLOAT:                 "float",
  mojom.HANDLE:                "mojo::Handle",
  mojom.DCPIPE:                "mojo::DataPipeConsumerHandle",
  mojom.DPPIPE:                "mojo::DataPipeProducerHandle",
  mojom.MSGPIPE:               "mojo::MessagePipeHandle",
  mojom.SHAREDBUFFER:          "mojo::SharedBufferHandle",
  mojom.NULLABLE_HANDLE:       "mojo::Handle",
  mojom.NULLABLE_DCPIPE:       "mojo::DataPipeConsumerHandle",
  mojom.NULLABLE_DPPIPE:       "mojo::DataPipeProducerHandle",
  mojom.NULLABLE_MSGPIPE:      "mojo::MessagePipeHandle",
  mojom.NULLABLE_SHAREDBUFFER: "mojo::SharedBufferHandle",
  mojom.INT64:                 "int64_t",
  mojom.UINT64:                "uint64_t",
  mojom.DOUBLE:                "double",
}

_kind_to_cpp_literal_suffix = {
  mojom.UINT8:        "U",
  mojom.UINT16:       "U",
  mojom.UINT32:       "U",
  mojom.FLOAT:        "f",
  mojom.UINT64:       "ULL",
}

def ConstantValue(constant):
  return ExpressionToText(constant.value, kind=constant.kind)

def DefaultValue(field):
  if field.default:
    if mojom.IsStructKind(field.kind):
      assert field.default == "default"
      return "%s::New()" % GetNameForKind(field.kind)
    return ExpressionToText(field.default, kind=field.kind)
  return ""

def NamespaceToArray(namespace):
  return namespace.split('.') if namespace else []

def GetNameForKind(kind, internal = False):
  parts = []
  if kind.imported_from:
    parts.extend(NamespaceToArray(kind.imported_from["namespace"]))
  if internal:
    parts.append("internal")
  if kind.parent_kind:
    parts.append(kind.parent_kind.name)
  parts.append(kind.name)
  return "::".join(parts)

def GetCppType(kind):
  if mojom.IsStructKind(kind):
    return "%s_Data*" % GetNameForKind(kind, internal=True)
  if mojom.IsAnyArrayKind(kind):
    return "mojo::internal::Array_Data<%s>*" % GetCppType(kind.kind)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return "mojo::MessagePipeHandle"
  if mojom.IsEnumKind(kind):
    return "int32_t"
  if mojom.IsStringKind(kind):
    return "mojo::internal::String_Data*"
  return _kind_to_cpp_type[kind]

def GetCppPodType(kind):
  if mojom.IsStringKind(kind):
    return "char*"
  return _kind_to_cpp_type[kind]

def GetCppArrayArgWrapperType(kind):
  if mojom.IsEnumKind(kind):
    return GetNameForKind(kind)
  if mojom.IsStructKind(kind):
    return "%sPtr" % GetNameForKind(kind)
  if mojom.IsAnyArrayKind(kind):
    return "mojo::Array<%s> " % GetCppArrayArgWrapperType(kind.kind)
  if mojom.IsInterfaceKind(kind):
    raise Exception("Arrays of interfaces not yet supported!")
  if mojom.IsInterfaceRequestKind(kind):
    raise Exception("Arrays of interface requests not yet supported!")
  if mojom.IsStringKind(kind):
    return "mojo::String"
  if mojom.IsHandleKind(kind):
    return "mojo::ScopedHandle"
  if mojom.IsDataPipeConsumerKind(kind):
    return "mojo::ScopedDataPipeConsumerHandle"
  if mojom.IsDataPipeProducerKind(kind):
    return "mojo::ScopedDataPipeProducerHandle"
  if mojom.IsMessagePipeKind(kind):
    return "mojo::ScopedMessagePipeHandle"
  if mojom.IsSharedBufferKind(kind):
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetCppResultWrapperType(kind):
  if mojom.IsEnumKind(kind):
    return GetNameForKind(kind)
  if mojom.IsStructKind(kind):
    return "%sPtr" % GetNameForKind(kind)
  if mojom.IsAnyArrayKind(kind):
    return "mojo::Array<%s>" % GetCppArrayArgWrapperType(kind.kind)
  if mojom.IsInterfaceKind(kind):
    return "%sPtr" % GetNameForKind(kind)
  if mojom.IsInterfaceRequestKind(kind):
    return "mojo::InterfaceRequest<%s>" % GetNameForKind(kind.kind)
  if mojom.IsStringKind(kind):
    return "mojo::String"
  if mojom.IsHandleKind(kind):
    return "mojo::ScopedHandle"
  if mojom.IsDataPipeConsumerKind(kind):
    return "mojo::ScopedDataPipeConsumerHandle"
  if mojom.IsDataPipeProducerKind(kind):
    return "mojo::ScopedDataPipeProducerHandle"
  if mojom.IsMessagePipeKind(kind):
    return "mojo::ScopedMessagePipeHandle"
  if mojom.IsSharedBufferKind(kind):
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetCppWrapperType(kind):
  if mojom.IsEnumKind(kind):
    return GetNameForKind(kind)
  if mojom.IsStructKind(kind):
    return "%sPtr" % GetNameForKind(kind)
  if mojom.IsAnyArrayKind(kind):
    return "mojo::Array<%s>" % GetCppArrayArgWrapperType(kind.kind)
  if mojom.IsInterfaceKind(kind):
    return "%sPtr" % GetNameForKind(kind)
  if mojom.IsInterfaceRequestKind(kind):
    raise Exception("InterfaceRequest fields not supported!")
  if mojom.IsStringKind(kind):
    return "mojo::String"
  if mojom.IsHandleKind(kind):
    return "mojo::ScopedHandle"
  if mojom.IsDataPipeConsumerKind(kind):
    return "mojo::ScopedDataPipeConsumerHandle"
  if mojom.IsDataPipeProducerKind(kind):
    return "mojo::ScopedDataPipeProducerHandle"
  if mojom.IsMessagePipeKind(kind):
    return "mojo::ScopedMessagePipeHandle"
  if mojom.IsSharedBufferKind(kind):
    return "mojo::ScopedSharedBufferHandle"
  return _kind_to_cpp_type[kind]

def GetCppConstWrapperType(kind):
  if mojom.IsStructKind(kind):
    return "%sPtr" % GetNameForKind(kind)
  if mojom.IsAnyArrayKind(kind):
    return "mojo::Array<%s>" % GetCppArrayArgWrapperType(kind.kind)
  if mojom.IsInterfaceKind(kind):
    return "%sPtr" % GetNameForKind(kind)
  if mojom.IsInterfaceRequestKind(kind):
    return "mojo::InterfaceRequest<%s>" % GetNameForKind(kind.kind)
  if mojom.IsEnumKind(kind):
    return GetNameForKind(kind)
  if mojom.IsStringKind(kind):
    return "const mojo::String&"
  if mojom.IsHandleKind(kind):
    return "mojo::ScopedHandle"
  if mojom.IsDataPipeConsumerKind(kind):
    return "mojo::ScopedDataPipeConsumerHandle"
  if mojom.IsDataPipeProducerKind(kind):
    return "mojo::ScopedDataPipeProducerHandle"
  if mojom.IsMessagePipeKind(kind):
    return "mojo::ScopedMessagePipeHandle"
  if mojom.IsSharedBufferKind(kind):
    return "mojo::ScopedSharedBufferHandle"
  if not kind in _kind_to_cpp_type:
    print "missing:", kind.spec
  return _kind_to_cpp_type[kind]

def GetCppFieldType(kind):
  if mojom.IsStructKind(kind):
    return ("mojo::internal::StructPointer<%s_Data>" %
        GetNameForKind(kind, internal=True))
  if mojom.IsAnyArrayKind(kind):
    return "mojo::internal::ArrayPointer<%s>" % GetCppType(kind.kind)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return "mojo::MessagePipeHandle"
  if mojom.IsEnumKind(kind):
    return GetNameForKind(kind)
  if mojom.IsStringKind(kind):
    return "mojo::internal::StringPointer"
  return _kind_to_cpp_type[kind]

def IsStructWithHandles(struct):
  for pf in struct.packed.packed_fields:
    if mojom.IsAnyHandleKind(pf.field.kind):
      return True
  return False

def TranslateConstants(token, kind):
  if isinstance(token, mojom.NamedValue):
    # Both variable and enum constants are constructed like:
    # Namespace::Struct::CONSTANT_NAME
    # For enums, CONSTANT_NAME is ENUM_NAME_ENUM_VALUE.
    name = []
    if token.imported_from:
      name.extend(NamespaceToArray(token.namespace))
    if token.parent_kind:
      name.append(token.parent_kind.name)
    if isinstance(token, mojom.EnumValue):
      name.append(
          "%s_%s" % (generator.CamelCaseToAllCaps(token.enum_name), token.name))
    else:
      name.append(token.name)
    return "::".join(name)
  return '%s%s' % (token, _kind_to_cpp_literal_suffix.get(kind, ''))

def ExpressionToText(value, kind=None):
  return TranslateConstants(value, kind)

def ShouldInlineStruct(struct):
  # TODO(darin): Base this on the size of the wrapper class.
  if len(struct.fields) > 4:
    return False
  for field in struct.fields:
    if mojom.IsMoveOnlyKind(field.kind):
      return False
  return True

def GetArrayValidateParams(kind):
  if not mojom.IsAnyArrayKind(kind) and not mojom.IsStringKind(kind):
    return "mojo::internal::NoValidateParams"

  if mojom.IsStringKind(kind):
    expected_num_elements = 0
    element_is_nullable = False
    element_validate_params = "mojo::internal::NoValidateParams"
  else:
    expected_num_elements = generator.ExpectedArraySize(kind)
    element_is_nullable = mojom.IsNullableKind(kind.kind)
    element_validate_params = GetArrayValidateParams(kind.kind)

  return "mojo::internal::ArrayValidateParams<%d, %s,\n%s> " % (
      expected_num_elements,
      'true' if element_is_nullable else 'false',
      element_validate_params)

_HEADER_SIZE = 8

class Generator(generator.Generator):

  cpp_filters = {
    "constant_value": ConstantValue,
    "cpp_const_wrapper_type": GetCppConstWrapperType,
    "cpp_field_type": GetCppFieldType,
    "cpp_pod_type": GetCppPodType,
    "cpp_result_type": GetCppResultWrapperType,
    "cpp_type": GetCppType,
    "cpp_wrapper_type": GetCppWrapperType,
    "default_value": DefaultValue,
    "expected_array_size": generator.ExpectedArraySize,
    "expression_to_text": ExpressionToText,
    "get_array_validate_params": GetArrayValidateParams,
    "get_name_for_kind": GetNameForKind,
    "get_pad": pack.GetPad,
    "has_callbacks": mojom.HasCallbacks,
    "should_inline": ShouldInlineStruct,
    "is_any_array_kind": mojom.IsAnyArrayKind,
    "is_enum_kind": mojom.IsEnumKind,
    "is_move_only_kind": mojom.IsMoveOnlyKind,
    "is_any_handle_kind": mojom.IsAnyHandleKind,
    "is_interface_kind": mojom.IsInterfaceKind,
    "is_interface_request_kind": mojom.IsInterfaceRequestKind,
    "is_nullable_kind": mojom.IsNullableKind,
    "is_object_kind": mojom.IsObjectKind,
    "is_string_kind": mojom.IsStringKind,
    "is_struct_with_handles": IsStructWithHandles,
    "struct_size": lambda ps: ps.GetTotalSize() + _HEADER_SIZE,
    "struct_from_method": generator.GetStructFromMethod,
    "response_struct_from_method": generator.GetResponseStructFromMethod,
    "stylize_method": generator.StudlyCapsToCamel,
    "to_all_caps": generator.CamelCaseToAllCaps,
  }

  def GetJinjaExports(self):
    return {
      "module": self.module,
      "namespace": self.module.namespace,
      "namespaces_as_array": NamespaceToArray(self.module.namespace),
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

  def GenerateFiles(self, args):
    self.Write(self.GenerateModuleHeader(), "%s.h" % self.module.name)
    self.Write(self.GenerateModuleInternalHeader(),
        "%s-internal.h" % self.module.name)
    self.Write(self.GenerateModuleSource(), "%s.cc" % self.module.name)
