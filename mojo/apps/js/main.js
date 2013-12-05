// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "console",
    "mojo/apps/js/bindings/connector",
    "mojo/apps/js/bindings/core",
    "mojo/apps/js/bindings/threading",
    "mojom/native_viewport",
    "mojom/gles2",
], function(console,
            connector,
            core,
            threading,
            nativeViewport,
            gles2) {

  function NativeViewportClientImpl() {
  }

  NativeViewportClientImpl.prototype =
      Object.create(nativeViewport.NativeViewportClientStub.prototype);

  NativeViewportClientImpl.prototype.didOpen = function() {
    console.log("NativeViewportClientImpl.prototype.DidOpen");
  };

  function GLES2ClientImpl() {
  }

  GLES2ClientImpl.prototype =
      Object.create(gles2.GLES2ClientStub.prototype);

  GLES2ClientImpl.prototype.didCreateContext = function(encoded,
                                                        width,
                                                        height) {
    console.log("GLES2ClientImpl.prototype.didCreateContext");
    // Need to call MojoGLES2MakeCurrent(encoded) in C++.
    // TODO(abarth): Should we handle some of this GL setup in C++?
  };

  GLES2ClientImpl.prototype.contextLost = function() {
    console.log("GLES2ClientImpl.prototype.contextLost");
  };

  var nativeViewportConnection = null;
  var gles2Connection = null;

  return function(handle) {
    nativeViewportConnection = new connector.Connection(
        handle,
        NativeViewportClientImpl,
        nativeViewport.NativeViewportProxy);

    var gles2Handles = core.createMessagePipe();
    gles2Connection = new connector.Connection(
        gles2Handles.handle0, GLES2ClientImpl, gles2.GLES2Proxy);

    nativeViewportConnection.remote.open();
    nativeViewportConnection.remote.createGLES2Context(gles2Handles.handle1);
  };
});
