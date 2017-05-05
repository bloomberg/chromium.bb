# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates JavaScript source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
import os
from mojom.generate.template_expander import UseJinja

_kind_to_javascript_default_value = {
  mojom.BOOL:                  "false",
  mojom.INT8:                  "0",
  mojom.UINT8:                 "0",
  mojom.INT16:                 "0",
  mojom.UINT16:                "0",
  mojom.INT32:                 "0",
  mojom.UINT32:                "0",
  mojom.FLOAT:                 "0",
  mojom.HANDLE:                "null",
  mojom.DCPIPE:                "null",
  mojom.DPPIPE:                "null",
  mojom.MSGPIPE:               "null",
  mojom.SHAREDBUFFER:          "null",
  mojom.NULLABLE_HANDLE:       "null",
  mojom.NULLABLE_DCPIPE:       "null",
  mojom.NULLABLE_DPPIPE:       "null",
  mojom.NULLABLE_MSGPIPE:      "null",
  mojom.NULLABLE_SHAREDBUFFER: "null",
  mojom.INT64:                 "0",
  mojom.UINT64:                "0",
  mojom.DOUBLE:                "0",
  mojom.STRING:                "null",
  mojom.NULLABLE_STRING:       "null"
}


def JavaScriptType(kind):
  name = []
  if kind.imported_from:
    name.append(kind.imported_from["unique_name"])
  if kind.parent_kind:
    name.append(kind.parent_kind.name)
  name.append(kind.name)
  return ".".join(name)


def JavaScriptDefaultValue(field):
  if field.default:
    if mojom.IsStructKind(field.kind):
      assert field.default == "default"
      return "new %s()" % JavaScriptType(field.kind)
    return ExpressionToText(field.default)
  if field.kind in mojom.PRIMITIVES:
    return _kind_to_javascript_default_value[field.kind]
  if mojom.IsStructKind(field.kind):
    return "null"
  if mojom.IsUnionKind(field.kind):
    return "null"
  if mojom.IsArrayKind(field.kind):
    return "null"
  if mojom.IsMapKind(field.kind):
    return "null"
  if mojom.IsInterfaceKind(field.kind):
    return "new %sPtr()" % JavaScriptType(field.kind)
  if mojom.IsInterfaceRequestKind(field.kind):
    return "new bindings.InterfaceRequest()"
  if mojom.IsAssociatedInterfaceKind(field.kind):
    return "new associatedBindings.AssociatedInterfacePtrInfo()"
  if mojom.IsAssociatedInterfaceRequestKind(field.kind):
    return "new associatedBindings.AssociatedInterfaceRequest()"
  if mojom.IsEnumKind(field.kind):
    return "0"
  raise Exception("No valid default: %s" % field)


def JavaScriptPayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = pack.GetPad(offset, 8)
  return offset + pad


_kind_to_codec_type = {
  mojom.BOOL:                  "codec.Uint8",
  mojom.INT8:                  "codec.Int8",
  mojom.UINT8:                 "codec.Uint8",
  mojom.INT16:                 "codec.Int16",
  mojom.UINT16:                "codec.Uint16",
  mojom.INT32:                 "codec.Int32",
  mojom.UINT32:                "codec.Uint32",
  mojom.FLOAT:                 "codec.Float",
  mojom.HANDLE:                "codec.Handle",
  mojom.DCPIPE:                "codec.Handle",
  mojom.DPPIPE:                "codec.Handle",
  mojom.MSGPIPE:               "codec.Handle",
  mojom.SHAREDBUFFER:          "codec.Handle",
  mojom.NULLABLE_HANDLE:       "codec.NullableHandle",
  mojom.NULLABLE_DCPIPE:       "codec.NullableHandle",
  mojom.NULLABLE_DPPIPE:       "codec.NullableHandle",
  mojom.NULLABLE_MSGPIPE:      "codec.NullableHandle",
  mojom.NULLABLE_SHAREDBUFFER: "codec.NullableHandle",
  mojom.INT64:                 "codec.Int64",
  mojom.UINT64:                "codec.Uint64",
  mojom.DOUBLE:                "codec.Double",
  mojom.STRING:                "codec.String",
  mojom.NULLABLE_STRING:       "codec.NullableString",
}


def CodecType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_codec_type[kind]
  if mojom.IsStructKind(kind):
    pointer_type = "NullablePointerTo" if mojom.IsNullableKind(kind) \
        else "PointerTo"
    return "new codec.%s(%s)" % (pointer_type, JavaScriptType(kind))
  if mojom.IsUnionKind(kind):
    return JavaScriptType(kind)
  if mojom.IsArrayKind(kind):
    array_type = "NullableArrayOf" if mojom.IsNullableKind(kind) else "ArrayOf"
    array_length = "" if kind.length is None else ", %d" % kind.length
    element_type = ElementCodecType(kind.kind)
    return "new codec.%s(%s%s)" % (array_type, element_type, array_length)
  if mojom.IsInterfaceKind(kind):
    return "new codec.%s(%sPtr)" % (
        "NullableInterface" if mojom.IsNullableKind(kind) else "Interface",
        JavaScriptType(kind))
  if mojom.IsInterfaceRequestKind(kind):
    return "codec.%s" % (
        "NullableInterfaceRequest" if mojom.IsNullableKind(kind)
                                   else "InterfaceRequest")
  if mojom.IsAssociatedInterfaceKind(kind):
    return "codec.%s" % (
        "NullableAssociatedInterfacePtrInfo" if mojom.IsNullableKind(kind)
                                             else "AssociatedInterfacePtrInfo")
  if mojom.IsAssociatedInterfaceRequestKind(kind):
    return "codec.%s" % (
        "NullableAssociatedInterfaceRequest" if mojom.IsNullableKind(kind)
                                             else "AssociatedInterfaceRequest")
  if mojom.IsEnumKind(kind):
    return "new codec.Enum(%s)" % JavaScriptType(kind)
  if mojom.IsMapKind(kind):
    map_type = "NullableMapOf" if mojom.IsNullableKind(kind) else "MapOf"
    key_type = ElementCodecType(kind.key_kind)
    value_type = ElementCodecType(kind.value_kind)
    return "new codec.%s(%s, %s)" % (map_type, key_type, value_type)
  raise Exception("No codec type for %s" % kind)


def ElementCodecType(kind):
  return "codec.PackedBool" if mojom.IsBoolKind(kind) else CodecType(kind)


def JavaScriptDecodeSnippet(kind):
  if (kind in mojom.PRIMITIVES or mojom.IsUnionKind(kind) or
      mojom.IsAnyInterfaceKind(kind)):
    return "decodeStruct(%s)" % CodecType(kind)
  if mojom.IsStructKind(kind):
    return "decodeStructPointer(%s)" % JavaScriptType(kind)
  if mojom.IsMapKind(kind):
    return "decodeMapPointer(%s, %s)" % \
        (ElementCodecType(kind.key_kind), ElementCodecType(kind.value_kind))
  if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
    return "decodeArrayPointer(codec.PackedBool)"
  if mojom.IsArrayKind(kind):
    return "decodeArrayPointer(%s)" % CodecType(kind.kind)
  if mojom.IsUnionKind(kind):
    return "decodeUnion(%s)" % CodecType(kind)
  if mojom.IsEnumKind(kind):
    return JavaScriptDecodeSnippet(mojom.INT32)
  raise Exception("No decode snippet for %s" % kind)


def JavaScriptEncodeSnippet(kind):
  if (kind in mojom.PRIMITIVES or mojom.IsUnionKind(kind) or
      mojom.IsAnyInterfaceKind(kind)):
    return "encodeStruct(%s, " % CodecType(kind)
  if mojom.IsUnionKind(kind):
    return "encodeStruct(%s, " % JavaScriptType(kind)
  if mojom.IsStructKind(kind):
    return "encodeStructPointer(%s, " % JavaScriptType(kind)
  if mojom.IsMapKind(kind):
    return "encodeMapPointer(%s, %s, " % \
        (ElementCodecType(kind.key_kind), ElementCodecType(kind.value_kind))
  if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
    return "encodeArrayPointer(codec.PackedBool, ";
  if mojom.IsArrayKind(kind):
    return "encodeArrayPointer(%s, " % CodecType(kind.kind)
  if mojom.IsEnumKind(kind):
    return JavaScriptEncodeSnippet(mojom.INT32)
  raise Exception("No encode snippet for %s" % kind)


def JavaScriptUnionDecodeSnippet(kind):
  if mojom.IsUnionKind(kind):
    return "decodeStructPointer(%s)" % JavaScriptType(kind)
  return JavaScriptDecodeSnippet(kind)


def JavaScriptUnionEncodeSnippet(kind):
  if mojom.IsUnionKind(kind):
    return "encodeStructPointer(%s, " % JavaScriptType(kind)
  return JavaScriptEncodeSnippet(kind)


def JavaScriptFieldOffset(packed_field):
  return "offset + codec.kStructHeaderSize + %s" % packed_field.offset


def JavaScriptNullableParam(field):
  return "true" if mojom.IsNullableKind(field.kind) else "false"


def GetArrayExpectedDimensionSizes(kind):
  expected_dimension_sizes = []
  while mojom.IsArrayKind(kind):
    expected_dimension_sizes.append(generator.ExpectedArraySize(kind) or 0)
    kind = kind.kind
  # Strings are serialized as variable-length arrays.
  if (mojom.IsStringKind(kind)):
    expected_dimension_sizes.append(0)
  return expected_dimension_sizes


def JavaScriptValidateArrayParams(field):
  nullable = JavaScriptNullableParam(field)
  element_kind = field.kind.kind
  element_size = pack.PackedField.GetSizeForKind(element_kind)
  expected_dimension_sizes = GetArrayExpectedDimensionSizes(
      field.kind)
  element_type = ElementCodecType(element_kind)
  return "%s, %s, %s, %s, 0" % \
      (element_size, element_type, nullable,
       expected_dimension_sizes)


def JavaScriptValidateEnumParams(field):
  return JavaScriptType(field.kind)

def JavaScriptValidateStructParams(field):
  nullable = JavaScriptNullableParam(field)
  struct_type = JavaScriptType(field.kind)
  return "%s, %s" % (struct_type, nullable)

def JavaScriptValidateUnionParams(field):
  nullable = JavaScriptNullableParam(field)
  union_type = JavaScriptType(field.kind)
  return "%s, %s" % (union_type, nullable)

def JavaScriptValidateMapParams(field):
  nullable = JavaScriptNullableParam(field)
  keys_type = ElementCodecType(field.kind.key_kind)
  values_kind = field.kind.value_kind;
  values_type = ElementCodecType(values_kind)
  values_nullable = "true" if mojom.IsNullableKind(values_kind) else "false"
  return "%s, %s, %s, %s" % \
      (nullable, keys_type, values_type, values_nullable)


def TranslateConstants(token):
  if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
    # Both variable and enum constants are constructed like:
    # NamespaceUid.Struct[.Enum].CONSTANT_NAME
    name = []
    if token.imported_from:
      name.append(token.imported_from["unique_name"])
    if token.parent_kind:
      name.append(token.parent_kind.name)
    if isinstance(token, mojom.EnumValue):
      name.append(token.enum.name)
    name.append(token.name)
    return ".".join(name)

  if isinstance(token, mojom.BuiltinValue):
    if token.value == "double.INFINITY" or token.value == "float.INFINITY":
      return "Infinity";
    if token.value == "double.NEGATIVE_INFINITY" or \
       token.value == "float.NEGATIVE_INFINITY":
      return "-Infinity";
    if token.value == "double.NAN" or token.value == "float.NAN":
      return "NaN";

  return token


def ExpressionToText(value):
  return TranslateConstants(value)

def IsArrayPointerField(field):
  return mojom.IsArrayKind(field.kind)

def IsEnumField(field):
  return mojom.IsEnumKind(field.kind)

def IsStringPointerField(field):
  return mojom.IsStringKind(field.kind)

def IsStructPointerField(field):
  return mojom.IsStructKind(field.kind)

def IsMapPointerField(field):
  return mojom.IsMapKind(field.kind)

def IsHandleField(field):
  return mojom.IsAnyHandleKind(field.kind)

def IsInterfaceField(field):
  return mojom.IsInterfaceKind(field.kind)

def IsInterfaceRequestField(field):
  return mojom.IsInterfaceRequestKind(field.kind)

def IsAssociatedInterfaceField(field):
  return mojom.IsAssociatedInterfaceKind(field.kind)

def IsAssociatedInterfaceRequestField(field):
  return mojom.IsAssociatedInterfaceRequestKind(field.kind)

def IsUnionField(field):
  return mojom.IsUnionKind(field.kind)

def IsBoolField(field):
  return mojom.IsBoolKind(field.kind)

def IsObjectField(field):
  return mojom.IsObjectKind(field.kind)

def IsAnyHandleOrInterfaceField(field):
  return mojom.IsAnyHandleOrInterfaceKind(field.kind)

def IsEnumField(field):
  return mojom.IsEnumKind(field.kind)

def GetRelativePath(module, base_module):
  return os.path.relpath(module.path, os.path.dirname(base_module.path))


class Generator(generator.Generator):
  def _GetParameters(self):
    return {
      "namespace": self.module.namespace,
      "imports": self._GetImports(),
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "module": self.module,
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "unions": self.GetUnions(),
      "use_new_js_bindings": self.use_new_js_bindings,
      "interfaces": self.GetInterfaces(),
      "imported_interfaces": self._GetImportedInterfaces(),
    }

  @staticmethod
  def GetTemplatePrefix():
    return "js_templates"

  def GetFilters(self):
    js_filters = {
      "decode_snippet": JavaScriptDecodeSnippet,
      "default_value": JavaScriptDefaultValue,
      "encode_snippet": JavaScriptEncodeSnippet,
      "expression_to_text": ExpressionToText,
      "field_offset": JavaScriptFieldOffset,
      "has_callbacks": mojom.HasCallbacks,
      "is_any_handle_or_interface_field": IsAnyHandleOrInterfaceField,
      "is_array_pointer_field": IsArrayPointerField,
      "is_associated_interface_field": IsAssociatedInterfaceField,
      "is_associated_interface_request_field":
          IsAssociatedInterfaceRequestField,
      "is_bool_field": IsBoolField,
      "is_enum_field": IsEnumField,
      "is_handle_field": IsHandleField,
      "is_interface_field": IsInterfaceField,
      "is_interface_request_field": IsInterfaceRequestField,
      "is_map_pointer_field": IsMapPointerField,
      "is_object_field": IsObjectField,
      "is_string_pointer_field": IsStringPointerField,
      "is_struct_pointer_field": IsStructPointerField,
      "is_union_field": IsUnionField,
      "js_type": JavaScriptType,
      "method_passes_associated_kinds": mojom.MethodPassesAssociatedKinds,
      "payload_size": JavaScriptPayloadSize,
      "get_relative_path": GetRelativePath,
      "stylize_method": generator.StudlyCapsToCamel,
      "union_decode_snippet": JavaScriptUnionDecodeSnippet,
      "union_encode_snippet": JavaScriptUnionEncodeSnippet,
      "validate_array_params": JavaScriptValidateArrayParams,
      "validate_enum_params": JavaScriptValidateEnumParams,
      "validate_map_params": JavaScriptValidateMapParams,
      "validate_nullable_params": JavaScriptNullableParam,
      "validate_struct_params": JavaScriptValidateStructParams,
      "validate_union_params": JavaScriptValidateUnionParams,
    }
    return js_filters

  @UseJinja("module.amd.tmpl")
  def _GenerateAMDModule(self):
    return self._GetParameters()

  def GenerateFiles(self, args):
    if self.variant:
      raise Exception("Variants not supported in JavaScript bindings.")

    self.Write(self._GenerateAMDModule(),
        self.MatchMojomFilePath("%s.js" % self.module.name))

  def _GetImports(self):
    used_names = set()
    for each_import in self.module.imports:
      simple_name = each_import["module_name"].split(".")[0]

      # Since each import is assigned a variable in JS, they need to have unique
      # names.
      unique_name = simple_name
      counter = 0
      while unique_name in used_names:
        counter += 1
        unique_name = simple_name + str(counter)

      used_names.add(unique_name)
      each_import["unique_name"] = unique_name + "$"
      counter += 1
    return self.module.imports

  def _GetImportedInterfaces(self):
    interface_to_import = {};
    for each_import in self.module.imports:
      for each_interface in each_import["module"].interfaces:
        name = each_interface.name
        interface_to_import[name] = each_import["unique_name"] + "." + name
    return interface_to_import;

