# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates JavaScript source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
import os
import urllib
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


def JavaScriptPayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = pack.GetPad(offset, 8)
  return offset + pad


def JavaScriptFieldOffset(packed_field):
  return "offset + codec.kStructHeaderSize + %s" % packed_field.offset


def GetArrayExpectedDimensionSizes(kind):
  expected_dimension_sizes = []
  while mojom.IsArrayKind(kind):
    expected_dimension_sizes.append(generator.ExpectedArraySize(kind) or 0)
    kind = kind.kind
  # Strings are serialized as variable-length arrays.
  if (mojom.IsStringKind(kind)):
    expected_dimension_sizes.append(0)
  return expected_dimension_sizes


def GetRelativeUrl(module, base_module):
  return urllib.pathname2url(
      os.path.relpath(module.path, os.path.dirname(base_module.path)))


class JavaScriptStylizer(generator.Stylizer):
  def StylizeConstant(self, mojom_name):
    return mojom_name

  def StylizeField(self, mojom_name):
    return generator.ToCamel(mojom_name, lower_initial=True)

  def StylizeStruct(self, mojom_name):
    return mojom_name

  def StylizeUnion(self, mojom_name):
    return mojom_name

  def StylizeParameter(self, mojom_name):
    return generator.ToCamel(mojom_name, lower_initial=True)

  def StylizeMethod(self, mojom_name):
    return generator.ToCamel(mojom_name, lower_initial=True)

  def StylizeEnumField(self, mojom_name):
    return mojom_name

  def StylizeEnum(self, mojom_name):
    return mojom_name

  def StylizeModule(self, mojom_namespace):
    return '.'.join(generator.ToCamel(word, lower_initial=True)
                        for word in mojom_namespace.split('.'))


class Generator(generator.Generator):
  def _GetParameters(self):
    return {
      "enums": self.module.enums,
      "imports": self.module.imports,
      "interfaces": self.module.interfaces,
      "kinds": self.module.kinds,
      "module": self.module,
      "structs": self.module.structs + self._GetStructsFromMethods(),
      "unions": self.module.unions,
    }

  @staticmethod
  def GetTemplatePrefix():
    return "js_templates"

  def GetFilters(self):
    js_filters = {
      "decode_snippet": self._JavaScriptDecodeSnippet,
      "default_value": self._JavaScriptDefaultValue,
      "encode_snippet": self._JavaScriptEncodeSnippet,
      "expression_to_text": self._ExpressionToText,
      "field_offset": JavaScriptFieldOffset,
      "get_relative_url": GetRelativeUrl,
      "has_callbacks": mojom.HasCallbacks,
      "is_any_handle_or_interface_kind": mojom.IsAnyHandleOrInterfaceKind,
      "is_array_kind": mojom.IsArrayKind,
      "is_associated_interface_kind": mojom.IsAssociatedInterfaceKind,
      "is_associated_interface_request_kind":
          mojom.IsAssociatedInterfaceRequestKind,
      "is_bool_kind": mojom.IsBoolKind,
      "is_enum_kind": mojom.IsEnumKind,
      "is_any_handle_kind": mojom.IsAnyHandleKind,
      "is_interface_kind": mojom.IsInterfaceKind,
      "is_interface_request_kind": mojom.IsInterfaceRequestKind,
      "is_map_kind": mojom.IsMapKind,
      "is_object_kind": mojom.IsObjectKind,
      "is_string_kind": mojom.IsStringKind,
      "is_struct_kind": mojom.IsStructKind,
      "is_union_kind": mojom.IsUnionKind,
      "js_type": self._JavaScriptType,
      "method_passes_associated_kinds": mojom.MethodPassesAssociatedKinds,
      "payload_size": JavaScriptPayloadSize,
      "to_camel": generator.ToCamel,
      "union_decode_snippet": self._JavaScriptUnionDecodeSnippet,
      "union_encode_snippet": self._JavaScriptUnionEncodeSnippet,
      "validate_array_params": self._JavaScriptValidateArrayParams,
      "validate_enum_params": self._JavaScriptValidateEnumParams,
      "validate_map_params": self._JavaScriptValidateMapParams,
      "validate_nullable_params": self._JavaScriptNullableParam,
      "validate_struct_params": self._JavaScriptValidateStructParams,
      "validate_union_params": self._JavaScriptValidateUnionParams,
    }
    return js_filters

  @UseJinja("module.amd.tmpl")
  def _GenerateAMDModule(self):
    return self._GetParameters()

  def GenerateFiles(self, args):
    if self.variant:
      raise Exception("Variants not supported in JavaScript bindings.")

    self.module.Stylize(JavaScriptStylizer())

    # TODO(crbug.com/795977): Change the media router extension to not mess with
    # the mojo namespace, so that namespaces such as "mojo.common.mojom" are not
    # affected and we can remove this method.
    self._SetUniqueNameForImports()

    self.Write(self._GenerateAMDModule(), "%s.js" % self.module.path)

  def _SetUniqueNameForImports(self):
    used_names = set()
    for each_import in self.module.imports:
      simple_name = os.path.basename(each_import.path).split(".")[0]

      # Since each import is assigned a variable in JS, they need to have unique
      # names.
      unique_name = simple_name
      counter = 0
      while unique_name in used_names:
        counter += 1
        unique_name = simple_name + str(counter)

      used_names.add(unique_name)
      each_import.unique_name = unique_name + "$"
      counter += 1

  def _JavaScriptType(self, kind):
    name = []
    if kind.module and kind.module.path != self.module.path:
      name.append(kind.module.unique_name)
    if kind.parent_kind:
      name.append(kind.parent_kind.name)
    name.append(kind.name)
    return ".".join(name)

  def _JavaScriptDefaultValue(self, field):
    if field.default:
      if mojom.IsStructKind(field.kind):
        assert field.default == "default"
        return "new %s()" % self._JavaScriptType(field.kind)
      return self._ExpressionToText(field.default)
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
      return "new %sPtr()" % self._JavaScriptType(field.kind)
    if mojom.IsInterfaceRequestKind(field.kind):
      return "new bindings.InterfaceRequest()"
    if mojom.IsAssociatedInterfaceKind(field.kind):
      return "new associatedBindings.AssociatedInterfacePtrInfo()"
    if mojom.IsAssociatedInterfaceRequestKind(field.kind):
      return "new associatedBindings.AssociatedInterfaceRequest()"
    if mojom.IsEnumKind(field.kind):
      return "0"
    raise Exception("No valid default: %s" % field)

  def _CodecType(self, kind):
    if kind in mojom.PRIMITIVES:
      return _kind_to_codec_type[kind]
    if mojom.IsStructKind(kind):
      pointer_type = "NullablePointerTo" if mojom.IsNullableKind(kind) \
          else "PointerTo"
      return "new codec.%s(%s)" % (pointer_type, self._JavaScriptType(kind))
    if mojom.IsUnionKind(kind):
      return self._JavaScriptType(kind)
    if mojom.IsArrayKind(kind):
      array_type = ("NullableArrayOf" if mojom.IsNullableKind(kind)
                                      else "ArrayOf")
      array_length = "" if kind.length is None else ", %d" % kind.length
      element_type = self._ElementCodecType(kind.kind)
      return "new codec.%s(%s%s)" % (array_type, element_type, array_length)
    if mojom.IsInterfaceKind(kind):
      return "new codec.%s(%sPtr)" % (
          "NullableInterface" if mojom.IsNullableKind(kind) else "Interface",
          self._JavaScriptType(kind))
    if mojom.IsInterfaceRequestKind(kind):
      return "codec.%s" % (
          "NullableInterfaceRequest" if mojom.IsNullableKind(kind)
                                     else "InterfaceRequest")
    if mojom.IsAssociatedInterfaceKind(kind):
      return "codec.%s" % ("NullableAssociatedInterfacePtrInfo"
          if mojom.IsNullableKind(kind) else "AssociatedInterfacePtrInfo")
    if mojom.IsAssociatedInterfaceRequestKind(kind):
      return "codec.%s" % ("NullableAssociatedInterfaceRequest"
          if mojom.IsNullableKind(kind) else "AssociatedInterfaceRequest")
    if mojom.IsEnumKind(kind):
      return "new codec.Enum(%s)" % self._JavaScriptType(kind)
    if mojom.IsMapKind(kind):
      map_type = "NullableMapOf" if mojom.IsNullableKind(kind) else "MapOf"
      key_type = self._ElementCodecType(kind.key_kind)
      value_type = self._ElementCodecType(kind.value_kind)
      return "new codec.%s(%s, %s)" % (map_type, key_type, value_type)
    raise Exception("No codec type for %s" % kind)

  def _ElementCodecType(self, kind):
    return ("codec.PackedBool" if mojom.IsBoolKind(kind)
                               else self._CodecType(kind))

  def _JavaScriptDecodeSnippet(self, kind):
    if (kind in mojom.PRIMITIVES or mojom.IsUnionKind(kind) or
        mojom.IsAnyInterfaceKind(kind)):
      return "decodeStruct(%s)" % self._CodecType(kind)
    if mojom.IsStructKind(kind):
      return "decodeStructPointer(%s)" % self._JavaScriptType(kind)
    if mojom.IsMapKind(kind):
      return "decodeMapPointer(%s, %s)" % (
          self._ElementCodecType(kind.key_kind),
          self._ElementCodecType(kind.value_kind))
    if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
      return "decodeArrayPointer(codec.PackedBool)"
    if mojom.IsArrayKind(kind):
      return "decodeArrayPointer(%s)" % self._CodecType(kind.kind)
    if mojom.IsUnionKind(kind):
      return "decodeUnion(%s)" % self._CodecType(kind)
    if mojom.IsEnumKind(kind):
      return self._JavaScriptDecodeSnippet(mojom.INT32)
    raise Exception("No decode snippet for %s" % kind)

  def _JavaScriptEncodeSnippet(self, kind):
    if (kind in mojom.PRIMITIVES or mojom.IsUnionKind(kind) or
        mojom.IsAnyInterfaceKind(kind)):
      return "encodeStruct(%s, " % self._CodecType(kind)
    if mojom.IsUnionKind(kind):
      return "encodeStruct(%s, " % self._JavaScriptType(kind)
    if mojom.IsStructKind(kind):
      return "encodeStructPointer(%s, " % self._JavaScriptType(kind)
    if mojom.IsMapKind(kind):
      return "encodeMapPointer(%s, %s, " % (
          self._ElementCodecType(kind.key_kind),
          self._ElementCodecType(kind.value_kind))
    if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
      return "encodeArrayPointer(codec.PackedBool, ";
    if mojom.IsArrayKind(kind):
      return "encodeArrayPointer(%s, " % self._CodecType(kind.kind)
    if mojom.IsEnumKind(kind):
      return self._JavaScriptEncodeSnippet(mojom.INT32)
    raise Exception("No encode snippet for %s" % kind)

  def _JavaScriptUnionDecodeSnippet(self, kind):
    if mojom.IsUnionKind(kind):
      return "decodeStructPointer(%s)" % self._JavaScriptType(kind)
    return self._JavaScriptDecodeSnippet(kind)

  def _JavaScriptUnionEncodeSnippet(self, kind):
    if mojom.IsUnionKind(kind):
      return "encodeStructPointer(%s, " % self._JavaScriptType(kind)
    return self._JavaScriptEncodeSnippet(kind)

  def _JavaScriptNullableParam(self, field):
    return "true" if mojom.IsNullableKind(field.kind) else "false"

  def _JavaScriptValidateArrayParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    element_kind = field.kind.kind
    element_size = pack.PackedField.GetSizeForKind(element_kind)
    expected_dimension_sizes = GetArrayExpectedDimensionSizes(
        field.kind)
    element_type = self._ElementCodecType(element_kind)
    return "%s, %s, %s, %s, 0" % \
        (element_size, element_type, nullable,
         expected_dimension_sizes)

  def _JavaScriptValidateEnumParams(self, field):
    return self._JavaScriptType(field.kind)

  def _JavaScriptValidateStructParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    struct_type = self._JavaScriptType(field.kind)
    return "%s, %s" % (struct_type, nullable)

  def _JavaScriptValidateUnionParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    union_type = self._JavaScriptType(field.kind)
    return "%s, %s" % (union_type, nullable)

  def _JavaScriptValidateMapParams(self, field):
    nullable = self._JavaScriptNullableParam(field)
    keys_type = self._ElementCodecType(field.kind.key_kind)
    values_kind = field.kind.value_kind;
    values_type = self._ElementCodecType(values_kind)
    values_nullable = "true" if mojom.IsNullableKind(values_kind) else "false"
    return "%s, %s, %s, %s" % \
        (nullable, keys_type, values_type, values_nullable)

  def _TranslateConstants(self, token):
    if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
      # Both variable and enum constants are constructed like:
      # NamespaceUid.Struct[.Enum].CONSTANT_NAME
      name = []
      if token.module and token.module.path != self.module.path:
        name.append(token.module.unique_name)
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

  def _ExpressionToText(self, value):
    return self._TranslateConstants(value)

  def _GetStructsFromMethods(self):
    result = []
    for interface in self.module.interfaces:
      for method in interface.methods:
        result.append(method.param_struct)
        if method.response_param_struct is not None:
          result.append(method.response_param_struct)
    return result
