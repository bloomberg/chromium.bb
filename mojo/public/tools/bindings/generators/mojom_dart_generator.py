# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates Dart source files from a mojom.Module."""

import mojom.generate.generator as generator
import mojom.generate.module as mojom
import mojom.generate.pack as pack
from mojom.generate.template_expander import UseJinja

_kind_to_dart_default_value = {
  mojom.BOOL:                  "false",
  mojom.INT8:                  "0",
  mojom.UINT8:                 "0",
  mojom.INT16:                 "0",
  mojom.UINT16:                "0",
  mojom.INT32:                 "0",
  mojom.UINT32:                "0",
  mojom.FLOAT:                 "0.0",
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
  mojom.DOUBLE:                "0.0",
  mojom.STRING:                "null",
  mojom.NULLABLE_STRING:       "null"
}


_kind_to_dart_decl_type = {
  mojom.BOOL:                  "bool",
  mojom.INT8:                  "int",
  mojom.UINT8:                 "int",
  mojom.INT16:                 "int",
  mojom.UINT16:                "int",
  mojom.INT32:                 "int",
  mojom.UINT32:                "int",
  mojom.FLOAT:                 "double",
  mojom.HANDLE:                "core.RawMojoHandle",
  mojom.DCPIPE:                "core.RawMojoHandle",
  mojom.DPPIPE:                "core.RawMojoHandle",
  mojom.MSGPIPE:               "core.RawMojoHandle",
  mojom.SHAREDBUFFER:          "core.RawMojoHandle",
  mojom.NULLABLE_HANDLE:       "core.RawMojoHandle",
  mojom.NULLABLE_DCPIPE:       "core.RawMojoHandle",
  mojom.NULLABLE_DPPIPE:       "core.RawMojoHandle",
  mojom.NULLABLE_MSGPIPE:      "core.RawMojoHandle",
  mojom.NULLABLE_SHAREDBUFFER: "core.RawMojoHandle",
  mojom.INT64:                 "int",
  mojom.UINT64:                "int",
  mojom.DOUBLE:                "double",
  mojom.STRING:                "String",
  mojom.NULLABLE_STRING:       "String"
}


def DartType(kind):
  if kind.imported_from:
    return kind.imported_from["unique_name"] + "." + kind.name
  return kind.name


def DartDefaultValue(field):
  if field.default:
    if mojom.IsStructKind(field.kind):
      assert field.default == "default"
      return "new %s()" % DartType(field.kind)
    return ExpressionToText(field.default)
  if field.kind in mojom.PRIMITIVES:
    return _kind_to_dart_default_value[field.kind]
  if mojom.IsStructKind(field.kind):
    return "null"
  if mojom.IsArrayKind(field.kind):
    return "null"
  if mojom.IsMapKind(field.kind):
    return "null"
  if mojom.IsInterfaceKind(field.kind) or \
     mojom.IsInterfaceRequestKind(field.kind):
    return _kind_to_dart_default_value[mojom.MSGPIPE]
  if mojom.IsEnumKind(field.kind):
    return "0"


def DartDeclType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_dart_decl_type[kind]
  if mojom.IsStructKind(kind):
    return DartType(kind)
  if mojom.IsArrayKind(kind):
    array_type = DartDeclType(kind.kind)
    return "List<" + array_type + ">"
  if mojom.IsMapKind(kind):
    key_type = DartDeclType(kind.key_kind)
    value_type = DartDeclType(kind.value_kind)
    return "Map<"+ key_type + ", " + value_type + ">"
  if mojom.IsInterfaceKind(kind) or \
     mojom.IsInterfaceRequestKind(kind):
    return _kind_to_dart_decl_type[mojom.MSGPIPE]
  if mojom.IsEnumKind(kind):
    return "int"

def DartPayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = pack.GetPad(offset, 8)
  return offset + pad


_kind_to_codec_type = {
  mojom.BOOL:                  "bindings.Uint8",
  mojom.INT8:                  "bindings.Int8",
  mojom.UINT8:                 "bindings.Uint8",
  mojom.INT16:                 "bindings.Int16",
  mojom.UINT16:                "bindings.Uint16",
  mojom.INT32:                 "bindings.Int32",
  mojom.UINT32:                "bindings.Uint32",
  mojom.FLOAT:                 "bindings.Float",
  mojom.HANDLE:                "bindings.Handle",
  mojom.DCPIPE:                "bindings.Handle",
  mojom.DPPIPE:                "bindings.Handle",
  mojom.MSGPIPE:               "bindings.Handle",
  mojom.SHAREDBUFFER:          "bindings.Handle",
  mojom.NULLABLE_HANDLE:       "bindings.NullableHandle",
  mojom.NULLABLE_DCPIPE:       "bindings.NullableHandle",
  mojom.NULLABLE_DPPIPE:       "bindings.NullableHandle",
  mojom.NULLABLE_MSGPIPE:      "bindings.NullableHandle",
  mojom.NULLABLE_SHAREDBUFFER: "bindings.NullableHandle",
  mojom.INT64:                 "bindings.Int64",
  mojom.UINT64:                "bindings.Uint64",
  mojom.DOUBLE:                "bindings.Double",
  mojom.STRING:                "bindings.MojoString",
  mojom.NULLABLE_STRING:       "bindings.NullableMojoString",
}


def CodecType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_codec_type[kind]
  if mojom.IsStructKind(kind):
    pointer_type = "NullablePointerTo" if mojom.IsNullableKind(kind) \
        else "PointerTo"
    return "new bindings.%s(%s)" % (pointer_type, DartType(kind))
  if mojom.IsArrayKind(kind):
    array_type = "NullableArrayOf" if mojom.IsNullableKind(kind) else "ArrayOf"
    array_length = "" if kind.length is None else ", %d" % kind.length
    element_type = ElementCodecType(kind.kind)
    return "new bindings.%s(%s%s)" % (array_type, element_type, array_length)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return CodecType(mojom.MSGPIPE)
  if mojom.IsEnumKind(kind):
    return _kind_to_codec_type[mojom.INT32]
  if mojom.IsMapKind(kind):
    map_type = "NullableMapOf" if mojom.IsNullableKind(kind) else "MapOf"
    key_type = ElementCodecType(kind.key_kind)
    value_type = ElementCodecType(kind.value_kind)
    return "new bindings.%s(%s, %s)" % (map_type, key_type, value_type)
  return kind

def ElementCodecType(kind):
  return "bindings.PackedBool" if mojom.IsBoolKind(kind) else CodecType(kind)

def DartDecodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return "decodeStruct(%s)" % CodecType(kind)
  if mojom.IsStructKind(kind):
    return "decodeStructPointer(%s)" % DartType(kind)
  if mojom.IsMapKind(kind):
    return "decodeMapPointer(%s, %s)" % \
        (ElementCodecType(kind.key_kind), ElementCodecType(kind.value_kind))
  if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
    return "decodeArrayPointer(bindings.PackedBool)"
  if mojom.IsArrayKind(kind):
    return "decodeArrayPointer(%s)" % CodecType(kind.kind)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return DartDecodeSnippet(mojom.MSGPIPE)
  if mojom.IsEnumKind(kind):
    return DartDecodeSnippet(mojom.INT32)


def DartEncodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return "encodeStruct(%s, " % CodecType(kind)
  if mojom.IsStructKind(kind):
    return "encodeStructPointer(%s, " % DartType(kind)
  if mojom.IsMapKind(kind):
    return "encodeMapPointer(%s, %s, " % \
        (ElementCodecType(kind.key_kind), ElementCodecType(kind.value_kind))
  if mojom.IsArrayKind(kind) and mojom.IsBoolKind(kind.kind):
    return "encodeArrayPointer(bindings.PackedBool, ";
  if mojom.IsArrayKind(kind):
    return "encodeArrayPointer(%s, " % CodecType(kind.kind)
  if mojom.IsInterfaceKind(kind) or mojom.IsInterfaceRequestKind(kind):
    return DartEncodeSnippet(mojom.MSGPIPE)
  if mojom.IsEnumKind(kind):
    return DartEncodeSnippet(mojom.INT32)


def TranslateConstants(token):
  if isinstance(token, (mojom.EnumValue, mojom.NamedValue)):
    # Both variable and enum constants are constructed like:
    # NamespaceUid.Struct.Enum_CONSTANT_NAME
    name = ""
    if token.imported_from:
      name = token.imported_from["unique_name"] + "."
    if token.parent_kind:
      name = name + token.parent_kind.name + "."
    if isinstance(token, mojom.EnumValue):
      name = name + token.enum.name + "_"
    return name + token.name

  if isinstance(token, mojom.BuiltinValue):
    if token.value == "double.INFINITY" or token.value == "float.INFINITY":
      return "double.INFINITY";
    if token.value == "double.NEGATIVE_INFINITY" or \
       token.value == "float.NEGATIVE_INFINITY":
      return "double.NEGATIVE_INFINITY";
    if token.value == "double.NAN" or token.value == "float.NAN":
      return "double.NAN";

  # Strip leading '+'.
  if token[0] == '+':
    token = token[1:]

  return token


def ExpressionToText(value):
  return TranslateConstants(value)


class Generator(generator.Generator):

  dart_filters = {
    "default_value": DartDefaultValue,
    "payload_size": DartPayloadSize,
    "decode_snippet": DartDecodeSnippet,
    "encode_snippet": DartEncodeSnippet,
    "expression_to_text": ExpressionToText,
    "dart_decl_type": DartDeclType,
    "stylize_method": generator.StudlyCapsToCamel,
  }

  def GetParameters(self):
    return {
      "namespace": self.module.namespace,
      "imports": self.GetImports(),
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "module": self.module,
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "interfaces": self.module.interfaces,
      "imported_interfaces": self.GetImportedInterfaces(),
      "imported_from": self.ImportedFrom(),
    }

  @UseJinja("dart_templates/module.lib.tmpl", filters=dart_filters)
  def GenerateLibModule(self):
    return self.GetParameters()

  def GenerateFiles(self, args):
    self.Write(self.GenerateLibModule(),
        self.MatchMojomFilePath("%s.dart" % self.module.name))

  def GetImports(self):
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
      each_import["unique_name"] = unique_name
      counter += 1
    return self.module.imports

  def GetImportedInterfaces(self):
    interface_to_import = {}
    for each_import in self.module.imports:
      for each_interface in each_import["module"].interfaces:
        name = each_interface.name
        interface_to_import[name] = each_import["unique_name"] + "." + name
    return interface_to_import

  def ImportedFrom(self):
    interface_to_import = {}
    for each_import in self.module.imports:
      for each_interface in each_import["module"].interfaces:
        name = each_interface.name
        interface_to_import[name] = each_import["unique_name"] + "."
    return interface_to_import
