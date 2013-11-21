# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates JS source files from a mojom.Module."""

import os
import mojom
import mojom_pack
import mojom_generator

from functools import partial
from template_expander import UseJinja

_kind_to_default_value = {
  mojom.BOOL:   "false",
  mojom.INT8:   "0",
  mojom.UINT8:  "0",
  mojom.INT16:  "0",
  mojom.UINT16: "0",
  mojom.INT32:  "0",
  mojom.UINT32: "0",
  mojom.FLOAT:  "0",
  mojom.HANDLE: "core.kInvalidHandle",
  mojom.INT64:  "0",
  mojom.UINT64: "0",
  mojom.DOUBLE: "0",
  mojom.STRING: '""',
}


def DefaultValue(field):
  if field.default:
    return field.default
  if field.kind in mojom.PRIMITIVES:
    return _kind_to_default_value[field.kind]
  if isinstance(field.kind, mojom.Struct):
    return "null";
  if isinstance(field.kind, mojom.Array):
    return "[]";


def PayloadSize(packed):
  packed_fields = packed.packed_fields
  if not packed_fields:
    return 0;
  last_field = packed_fields[-1]
  offset = last_field.offset + last_field.size
  pad = mojom_pack.GetPad(offset, 8)
  return offset + pad;


_kind_to_javascript_type = {
  mojom.BOOL:   "codec.Uint8",
  mojom.INT8:   "codec.Int8",
  mojom.UINT8:  "codec.Uint8",
  mojom.INT16:  "codec.Int16",
  mojom.UINT16: "codec.Uint16",
  mojom.INT32:  "codec.Int32",
  mojom.UINT32: "codec.Uint32",
  mojom.FLOAT:  "codec.Float",
  mojom.HANDLE: "codec.Handle",
  mojom.INT64:  "codec.Int64",
  mojom.UINT64: "codec.Uint64",
  mojom.DOUBLE: "codec.Double",
  mojom.STRING: "codec.String",
}


def GetJavaScriptType(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_javascript_type[kind]
  if isinstance(kind, mojom.Struct):
    return "new codec.PointerTo(%s)" % GetJavaScriptType(kind.name)
  if isinstance(kind, mojom.Array):
    return "new codec.ArrayOf(%s)" % GetJavaScriptType(kind.kind)
  return kind


_kind_to_decode_snippet = {
  mojom.BOOL:   "read8() & 1",
  mojom.INT8:   "read8()",
  mojom.UINT8:  "read8()",
  mojom.INT16:  "read16()",
  mojom.UINT16: "read16()",
  mojom.INT32:  "read32()",
  mojom.UINT32: "read32()",
  mojom.FLOAT:  "decodeFloat()",
  mojom.HANDLE: "decodeHandle()",
  mojom.INT64:  "read64()",
  mojom.UINT64: "read64()",
  mojom.DOUBLE: "decodeDouble()",
  mojom.STRING: "decodeStringPointer()",
}


def DecodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_decode_snippet[kind]
  if isinstance(kind, mojom.Struct):
    return "decodeStructPointer(%s)" % GetJavaScriptType(kind.name);
  if isinstance(kind, mojom.Array):
    return "decodeArrayPointer(%s)" % GetJavaScriptType(kind.kind);


_kind_to_encode_snippet = {
  mojom.BOOL:   "write8(1 & ",
  mojom.INT8:   "write8(",
  mojom.UINT8:  "write8(",
  mojom.INT16:  "write16(",
  mojom.UINT16: "write16(",
  mojom.INT32:  "write32(",
  mojom.UINT32: "write32(",
  mojom.FLOAT:  "encodeFloat(",
  mojom.HANDLE: "encodeHandle(",
  mojom.INT64:  "write64(",
  mojom.UINT64: "write64(",
  mojom.DOUBLE: "encodeDouble(",
  mojom.STRING: "encodeStringPointer(",
}


def EncodeSnippet(kind):
  if kind in mojom.PRIMITIVES:
    return _kind_to_encode_snippet[kind]
  if isinstance(kind, mojom.Struct):
    return "encodeStructPointer(%s, " % GetJavaScriptType(kind.name);
  if isinstance(kind, mojom.Array):
    return "encodeArrayPointer(%s, " % GetJavaScriptType(kind.kind);


def GetStructInfo(exported, struct):
  packed_struct = mojom_pack.PackedStruct(struct)
  return {
    "name": struct.name,
    "packed": packed_struct,
    "bytes": mojom_pack.GetByteLayout(packed_struct),
    "exported": exported,
  }


class JSGenerator(mojom_generator.Generator):
  filters = {
    "default_value": DefaultValue,
    "payload_size": PayloadSize,
    "decode_snippet": DecodeSnippet,
    "encode_snippet": EncodeSnippet,
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

  @UseJinja("js_templates/module.js.tmpl", filters=filters)
  def GenerateModule(self):
    return {
      "structs": self.GetStructs() + self.GetStructsFromMethods(),
      "interfaces": self.module.interfaces,
    }

  def Write(self, contents):
    if self.output_dir is None:
      print contents
      return
    filename = "%s.js" % self.module.name
    with open(os.path.join(self.output_dir, filename), "w+") as f:
      f.write(contents)

  def GenerateFiles(self):
    self.Write(self.GenerateModule())
