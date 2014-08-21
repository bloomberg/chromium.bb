# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates JavaScript source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
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
  mojom.STRING:                '""',
  mojom.NULLABLE_STRING:       '""'
}


def JavaScriptType(kind):
  if kind.imported_from:
    return kind.imported_from["unique_name"] + "." + kind.name
  return kind.name


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
  if mojom.IsAnyArrayKind(field.kind):
    return "null"
  if mojom.IsInterfaceKind(field.kind) or \
     mojom.IsInterfaceRequestKind(field.kind):
    return _kind_to_javascript_default_value[mojom.MSGPIPE]
  if mojom.IsEnumKind(field.kind):
    return "0"


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
  if mojom.IsAnyArrayKind(kind):
    array_type = "NullableArrayOf" if mojom.IsNullableKind(kind) else "ArrayOf"
    element_type = "codec.PackedBool" if mojom.IsBoolKind(kind.kind) \
        else CodecType(kind.kind)
    return "new codec.%s(%s)" % (array_type, element_type)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return CodecType(mojom.MSGPIPE)
  if mojom.IsEnumKind(kind):
    return _kind_to_codec_type[mojom.INT32]
  return kind


def JavaScriptDecodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return "decodeStruct(%s)" % CodecType(kind)
  if mojom.IsStructKind(kind):
    return "decodeStructPointer(%s)" % JavaScriptType(kind)
  if mojom.IsAnyArrayKind(kind) and mojom.IsBoolKind(kind.kind):
    return "decodeArrayPointer(codec.PackedBool)"
  if mojom.IsAnyArrayKind(kind):
    return "decodeArrayPointer(%s)" % CodecType(kind.kind)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return JavaScriptDecodeSnippet(mojom.MSGPIPE)
  if mojom.IsEnumKind(kind):
    return JavaScriptDecodeSnippet(mojom.INT32)


def JavaScriptEncodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return "encodeStruct(%s, " % CodecType(kind)
  if mojom.IsStructKind(kind):
    return "encodeStructPointer(%s, " % JavaScriptType(kind)
  if mojom.IsAnyArrayKind(kind) and mojom.IsBoolKind(kind.kind):
    return "encodeArrayPointer(codec.PackedBool, ";
  if mojom.IsAnyArrayKind(kind):
    return "encodeArrayPointer(%s, " % CodecType(kind.kind)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return JavaScriptEncodeSnippet(mojom.MSGPIPE)
  if mojom.IsEnumKind(kind):
    return JavaScriptEncodeSnippet(mojom.INT32)


def JavaScriptFieldOffset(packed_field):
  return "offset + codec.kStructHeaderSize + %s" % packed_field.offset


def JavaScriptNullableParam(packed_field):
  return "true" if mojom.IsNullableKind(packed_field.field.kind) else "false"


def JavaScriptValidateArrayParams(packed_field):
  nullable = JavaScriptNullableParam(packed_field)
  field_offset = JavaScriptFieldOffset(packed_field)
  element_kind = packed_field.field.kind.kind
  element_size = pack.PackedField.GetSizeForKind(element_kind)
  element_count = generator.ExpectedArraySize(packed_field.field.kind)
  element_type = "codec.PackedBool" if mojom.IsBoolKind(element_kind) \
      else CodecType(element_kind)
  return "%s, %s, %s, %s, %s" % \
      (field_offset, element_size, element_count, element_type, nullable)


def JavaScriptValidateStructParams(packed_field):
  nullable = JavaScriptNullableParam(packed_field)
  field_offset = JavaScriptFieldOffset(packed_field)
  struct_type = JavaScriptType(packed_field.field.kind)
  return "%s, %s, %s" % (field_offset, struct_type, nullable)


def JavaScriptValidateStringParams(packed_field):
  nullable = JavaScriptNullableParam(packed_field)
  return "%s, %s" % (JavaScriptFieldOffset(packed_field), nullable)


def JavaScriptValidateHandleParams(packed_field):
  nullable = JavaScriptNullableParam(packed_field)
  field_offset = JavaScriptFieldOffset(packed_field)
  return "%s, %s" % (field_offset, nullable)


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
      name.append(token.enum_name)
    name.append(token.name)
    return ".".join(name)
  return token


def ExpressionToText(value):
  return TranslateConstants(value)


def IsArrayPointerField(field):
  return mojom.IsAnyArrayKind(field.kind)

def IsStringPointerField(field):
  return mojom.IsStringKind(field.kind)

def IsStructPointerField(field):
  return mojom.IsStructKind(field.kind)

def IsHandleField(field):
  return mojom.IsAnyHandleKind(field.kind)


class Generator(generator.Generator):

  js_filters = {
    "default_value": JavaScriptDefaultValue,
    "payload_size": JavaScriptPayloadSize,
    "decode_snippet": JavaScriptDecodeSnippet,
    "encode_snippet": JavaScriptEncodeSnippet,
    "expression_to_text": ExpressionToText,
    "field_offset": JavaScriptFieldOffset,
    "has_callbacks": mojom.HasCallbacks,
    "is_array_pointer_field": IsArrayPointerField,
    "is_struct_pointer_field": IsStructPointerField,
    "is_string_pointer_field": IsStringPointerField,
    "is_handle_field": IsHandleField,
    "js_type": JavaScriptType,
    "stylize_method": generator.StudlyCapsToCamel,
    "validate_array_params": JavaScriptValidateArrayParams,
    "validate_handle_params": JavaScriptValidateHandleParams,
    "validate_string_params": JavaScriptValidateStringParams,
    "validate_struct_params": JavaScriptValidateStructParams,
  }

  @UseJinja("js_templates/module.js.tmpl", filters=js_filters)
  def GenerateJsModule(self):
    return {
      "namespace": self.module.namespace,
      "imports": self.GetImports(),
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "module": self.module,
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "interfaces": self.module.interfaces,
    }

  def GenerateFiles(self, args):
    self.Write(self.GenerateJsModule(), "%s.js" % self.module.name)

  def GetImports(self):
    # Since each import is assigned a variable in JS, they need to have unique
    # names.
    counter = 1
    for each in self.module.imports:
      each["unique_name"] = "import" + str(counter)
      counter += 1
    return self.module.imports
