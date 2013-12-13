// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "console",
    "mojo/apps/js/bindings/connector",
    "mojo/apps/js/bindings/core",
    "mojo/apps/js/bindings/gl",
    "mojo/apps/js/bindings/threading",
    "mojom/native_viewport",
    "mojom/gles2",
], function(console,
            connector,
            core,
            gljs,
            threading,
            nativeViewport,
            gles2) {

  const VERTEX_SHADER_SOURCE =
      "uniform mat4 u_mvpMatrix;                   \n" +
      "attribute vec4 a_position;                  \n" +
      "void main()                                 \n" +
      "{                                           \n" +
      "   gl_Position = u_mvpMatrix * a_position;  \n" +
      "}                                           \n";

  function NativeViewportClientImpl() {
  }

  // TODO(aa): It is a bummer to need this stub object in JavaScript. We should
  // have a 'client' object that contains both the sending and receiving bits of
  // the client side of the interface. Since JS is loosely typed, we do not need
  // a separate base class to inherit from to receive callbacks.
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
    var gl = new gljs.Context(encoded, width, height);

    var shader = gl.createShader(gl.VERTEX_SHADER);
    console.log("shader is: ", String(shader));
    gl.shaderSource(shader, VERTEX_SHADER_SOURCE);
    gl.compileShader(shader);
    console.log("all done");
  };

  GLES2ClientImpl.prototype.contextLost = function() {
    console.log("GLES2ClientImpl.prototype.contextLost");
  };

  return function(handle) {
    var nativeViewportConnection = new connector.Connection(
        handle,
        NativeViewportClientImpl,
        nativeViewport.NativeViewportProxy);

    var gles2Handles = core.createMessagePipe();
    var gles2Connection = new connector.Connection(
        gles2Handles.handle0, GLES2ClientImpl, gles2.GLES2Proxy);

    nativeViewportConnection.remote.open();
    nativeViewportConnection.remote.createGLES2Context(gles2Handles.handle1);
  };
});
