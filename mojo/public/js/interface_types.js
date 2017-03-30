// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/interface_types", [
  "mojo/public/js/core",
], function(core) {

  // Constants ----------------------------------------------------------------
  var kInterfaceIdNamespaceMask = 0x80000000;
  var kMasterInterfaceId = 0x00000000;
  var kInvalidInterfaceId = 0xFFFFFFFF;

  // ---------------------------------------------------------------------------

  function InterfacePtrInfo(handle, version) {
    this.handle = handle;
    this.version = version;
  }

  InterfacePtrInfo.prototype.isValid = function() {
    return core.isHandle(this.handle);
  };

  InterfacePtrInfo.prototype.close = function() {
    if (!this.isValid())
      return;

    core.close(this.handle);
    this.handle = null;
    this.version = 0;
  };

  // ---------------------------------------------------------------------------

  function InterfaceRequest(handle) {
    this.handle = handle;
  }

  InterfaceRequest.prototype.isValid = function() {
    return core.isHandle(this.handle);
  };

  InterfaceRequest.prototype.close = function() {
    if (!this.isValid())
      return;

    core.close(this.handle);
    this.handle = null;
  };

  function isMasterInterfaceId(interfaceId) {
    return interfaceId === kMasterInterfaceId;
  }

  function isValidInterfaceId(interfaceId) {
    return interfaceId !== kInvalidInterfaceId;
  }

  var exports = {};
  exports.InterfacePtrInfo = InterfacePtrInfo;
  exports.InterfaceRequest = InterfaceRequest;
  exports.isMasterInterfaceId = isMasterInterfaceId;
  exports.isValidInterfaceId = isValidInterfaceId;
  exports.kInvalidInterfaceId = kInvalidInterfaceId;
  exports.kMasterInterfaceId = kMasterInterfaceId;
  exports.kInterfaceIdNamespaceMask = kInterfaceIdNamespaceMask;

  return exports;
});
