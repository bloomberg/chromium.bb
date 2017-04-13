// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/associated_bindings", [
  "mojo/public/js/core",
  "mojo/public/js/interface_types",
  "mojo/public/js/lib/interface_endpoint_client",
  "mojo/public/js/lib/interface_endpoint_handle",
], function(core, types, interfaceEndpointClient, interfaceEndpointHandle) {

  var exports = {};
  exports.AssociatedInterfacePtrInfo = types.AssociatedInterfacePtrInfo;
  exports.AssociatedInterfaceRequest = types.AssociatedInterfaceRequest;

  return exports;
});
