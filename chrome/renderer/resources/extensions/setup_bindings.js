// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sgb = requireNative('schema_generated_bindings');

require('webstore');
require('json_schema');
require('event_bindings');
require('miscellaneous_bindings');
require('schema_generated_bindings');
require('apitest');

// Load the custom bindings for each API.
sgb.GetExtensionAPIDefinition().forEach(function(apiDef) {
  require(apiDef.namespace);
});
