# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates JavaScript source files from a mojom.Module."""

from generate import mojom
from generate import mojom_pack
from generate import mojom_generator

from generate.template_expander import UseJinja

_kind_to_javascript_default_value = {
  mojom.BOOL:         "false",
  mojom.INT8:         "0",
  mojom.UINT8:        "0",
  mojom.INT16:        "0",
  mojom.UINT16:       "0",
  mojom.INT32:        "0",
  mojom.UINT32:       "0",
  mojom.FLOAT:        "0",
  mojom.HANDLE:       "core.kInvalidHandle",
  mojom.DCPIPE:       "core.kInvalidHandle",
  mojom.DPPIPE:       "core.kInvalidHandle",
  mojom.MSGPIPE:      "core.kInvalidHandle",
  mojom.SHAREDBUFFER: "core.kInvalidHandle",
  mojom.INT64:        "0",
  mojom.UINT64:       "0",
  mojom.DOUBLE:       "0",
  mojom.STRING:       '""',
}


def JavaScriptDefaultValue(field):
  if field.default:
    raise Exception("Default values should've been handled in jinja.")
  if field.kind in mojom.PRIMITIVES:
    return _kind_to_javascript_default_value[field.kind]
  if isinstance(field.kind, mojom.Struct):
    return "null";
  if isinstance(field.kind, mojom.Array):
    return "[]";
  if isinstance(field.kind, mojom.Interface):
    return _kind_to_javascript_default_value[mojom.MSGPIPE]
  if isinstance(field.kind, mojom.Enum):
    return "0"


def JavaScriptPayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0;
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = mojom_pack.GetPad(offset, 8)
  return offset + pad;


_kind_to_javascript_type = {
  mojom.BOOL:         "codec.Uint8",
  mojom.INT8:         "codec.Int8",
  mojom.UINT8:        "codec.Uint8",
  mojom.INT16:        "codec.Int16",
  mojom.UINT16:       "codec.Uint16",
  mojom.INT32:        "codec.Int32",
  mojom.UINT32:       "codec.Uint32",
  mojom.FLOAT:        "codec.Float",
  mojom.HANDLE:       "codec.Handle",
  mojom.DCPIPE:       "codec.Handle",
  mojom.DPPIPE:       "codec.Handle",
  mojom.MSGPIPE:      "codec.Handle",
  mojom.SHAREDBUFFER: "codec.Handle",
  mojom.INT64:        "codec.Int64",
  mojom.UINT64:       "codec.Uint64",
  mojom.DOUBLE:       "codec.Double",
  mojom.STRING:       "codec.String",
}


def GetJavaScriptType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_javascript_type[kind]
  if isinstance(kind, mojom.Struct):
    return "new codec.PointerTo(%s)" % GetJavaScriptType(kind.name)
  if isinstance(kind, mojom.Array):
    return "new codec.ArrayOf(%s)" % GetJavaScriptType(kind.kind)
  if isinstance(kind, mojom.Interface):
    return GetJavaScriptType(mojom.MSGPIPE)
  if isinstance(kind, mojom.Enum):
    return _kind_to_javascript_type[mojom.INT32]
  return kind


_kind_to_javascript_decode_snippet = {
  mojom.BOOL:         "read8() & 1",
  mojom.INT8:         "read8()",
  mojom.UINT8:        "read8()",
  mojom.INT16:        "read16()",
  mojom.UINT16:       "read16()",
  mojom.INT32:        "read32()",
  mojom.UINT32:       "read32()",
  mojom.FLOAT:        "decodeFloat()",
  mojom.HANDLE:       "decodeHandle()",
  mojom.DCPIPE:       "decodeHandle()",
  mojom.DPPIPE:       "decodeHandle()",
  mojom.MSGPIPE:      "decodeHandle()",
  mojom.SHAREDBUFFER: "decodeHandle()",
  mojom.INT64:        "read64()",
  mojom.UINT64:       "read64()",
  mojom.DOUBLE:       "decodeDouble()",
  mojom.STRING:       "decodeStringPointer()",
}


def JavaScriptDecodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_javascript_decode_snippet[kind]
  if isinstance(kind, mojom.Struct):
    return "decodeStructPointer(%s)" % GetJavaScriptType(kind.name);
  if isinstance(kind, mojom.Array):
    return "decodeArrayPointer(%s)" % GetJavaScriptType(kind.kind);
  if isinstance(kind, mojom.Interface):
    return JavaScriptDecodeSnippet(mojom.MSGPIPE)
  if isinstance(kind, mojom.Enum):
    return _kind_to_javascript_decode_snippet[mojom.INT32]


_kind_to_javascript_encode_snippet = {
  mojom.BOOL:         "write8(1 & ",
  mojom.INT8:         "write8(",
  mojom.UINT8:        "write8(",
  mojom.INT16:        "write16(",
  mojom.UINT16:       "write16(",
  mojom.INT32:        "write32(",
  mojom.UINT32:       "write32(",
  mojom.FLOAT:        "encodeFloat(",
  mojom.HANDLE:       "encodeHandle(",
  mojom.DCPIPE:       "encodeHandle(",
  mojom.DPPIPE:       "encodeHandle(",
  mojom.MSGPIPE:      "encodeHandle(",
  mojom.SHAREDBUFFER: "encodeHandle(",
  mojom.INT64:        "write64(",
  mojom.UINT64:       "write64(",
  mojom.DOUBLE:       "encodeDouble(",
  mojom.STRING:       "encodeStringPointer(",
}


def JavaScriptEncodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_javascript_encode_snippet[kind]
  if isinstance(kind, mojom.Struct):
    return "encodeStructPointer(%s, " % GetJavaScriptType(kind.name);
  if isinstance(kind, mojom.Array):
    return "encodeArrayPointer(%s, " % GetJavaScriptType(kind.kind);
  if isinstance(kind, mojom.Interface):
    return JavaScriptEncodeSnippet(mojom.MSGPIPE)
  if isinstance(kind, mojom.Enum):
    return _kind_to_javascript_encode_snippet[mojom.INT32]

def TranslateConstants(token, module):
  if isinstance(token, mojom.Constant):
    # Enum constants are constructed like:
    # NamespaceUid.Struct_Enum.FIELD_NAME
    name = []
    if token.imported_from:
      name.append(token.imported_from["unique_name"])
    if token.parent_kind:
      name.append(token.parent_kind.name + "_" + token.name[0])
    else:
      name.append(token.name[0])
    name.append(token.name[1])
    return ".".join(name)
  return token


def ExpressionToText(value, module):
  if value[0] != "EXPRESSION":
    raise Exception("Expected EXPRESSION, got" + value)
  return "".join(mojom_generator.ExpressionMapper(value,
      lambda token: TranslateConstants(token, module)))


def JavascriptType(kind):
  if kind.imported_from:
    return kind.imported_from["unique_name"] + "." + kind.name
  return kind.name


class Generator(mojom_generator.Generator):

  js_filters = {
    "default_value": JavaScriptDefaultValue,
    "payload_size": JavaScriptPayloadSize,
    "decode_snippet": JavaScriptDecodeSnippet,
    "encode_snippet": JavaScriptEncodeSnippet,
    "expression_to_text": ExpressionToText,
    "is_object_kind": mojom_generator.IsObjectKind,
    "is_string_kind": mojom_generator.IsStringKind,
    "is_array_kind": lambda kind: isinstance(kind, mojom.Array),
    "js_type": JavascriptType,
    "stylize_method": mojom_generator.StudlyCapsToCamel,
    "verify_token_type": mojom_generator.VerifyTokenType,
  }

  @UseJinja("js_templates/module.js.tmpl", filters=js_filters)
  def GenerateJsModule(self):
    return {
      "imports": self.GetImports(),
      "kinds": self.module.kinds,
      "enums": self.module.enums,
      "module": self.module,
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "interfaces": self.module.interfaces,
    }

  def GenerateFiles(self):
    self.Write(self.GenerateJsModule(), "%s.js" % self.module.name)

  def GetImports(self):
    # Since each import is assigned a variable in JS, they need to have unique
    # names.
    counter = 1
    for each in self.module.imports:
      each["unique_name"] = "import" + str(counter)
      counter += 1
    return self.module.imports
