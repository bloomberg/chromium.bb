// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/js_only_v8_extensions.h"

#include "chrome/renderer/extensions/extension_base.h"
#include "grit/renderer_resources.h"

// JsonSchemaJsV8Extension
const char* JsonSchemaJsV8Extension::kName = "chrome/jsonschema";
v8::Extension* JsonSchemaJsV8Extension::Get() {
  static v8::Extension* extension = new ExtensionBase(
      kName, ExtensionBase::GetStringResource(IDR_JSON_SCHEMA_JS), 0, NULL,
      NULL);
  return extension;
}

// ExtensionApiTestV8Extension
const char* ExtensionApiTestV8Extension::kName = "chrome/extensionapitest";
v8::Extension* ExtensionApiTestV8Extension::Get() {
  static v8::Extension* extension = new ExtensionBase(
      kName, ExtensionBase::GetStringResource(IDR_EXTENSION_APITEST_JS), 0,
      NULL, NULL);
  return extension;
}
