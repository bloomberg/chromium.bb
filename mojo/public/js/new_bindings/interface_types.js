// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  // ---------------------------------------------------------------------------

  function InterfacePtrInfo(handle, version) {
    this.handle = handle;
    this.version = version;
  }

  InterfacePtrInfo.prototype.isValid = function() {
    return this.handle instanceof MojoHandle;
  };

  InterfacePtrInfo.prototype.close = function() {
    if (!this.isValid())
      return;

    this.handle.close();
    this.handle = null;
    this.version = 0;
  };

  // ---------------------------------------------------------------------------

  function InterfaceRequest(handle) {
    this.handle = handle;
  }

  InterfaceRequest.prototype.isValid = function() {
    return this.handle instanceof MojoHandle;
  };

  InterfaceRequest.prototype.close = function() {
    if (!this.isValid())
      return;

    this.handle.close();
    this.handle = null;
  };

  mojo.InterfacePtrInfo = InterfacePtrInfo;
  mojo.InterfaceRequest = InterfaceRequest;
})();
