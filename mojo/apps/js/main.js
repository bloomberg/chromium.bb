// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    'console',
    'mojo/apps/js/bindings/connector',
    'mojo/apps/js/bindings/core',
    'mojo/apps/js/bindings/gl',
    'mojo/apps/js/bindings/threading',
    'mojom/native_viewport',
    'mojom/gles2',
    'mojom/shell',
], function(console,
            connector,
            core,
            gljs,
            threading,
            nativeViewport,
            gles2,
            shell) {

  const VERTEX_SHADER_SOURCE = [
    'uniform mat4 u_mvpMatrix;',
    'attribute vec4 a_position;',
    'void main()',
    '{',
    '   gl_Position = u_mvpMatrix * a_position;',
    '}'
  ].join('\n');

  const FRAGMENT_SHADER_SOURCE = [
    'precision mediump float;',
    'void main()',
    '{',
    '  gl_FragColor = vec4( 0.0, 1.0, 0.0, 1.0 );',
    '}'
  ].join('\n');

  function ESMatrix() {
    this.m = new Float32Array(16);
  }

  ESMatrix.prototype.loadZero = function() {
    for (var i = 0; i < this.m.length; i++) {
      this.m[i] = 0;
    }
  };

  ESMatrix.prototype.loadIdentity = function() {
    this.loadZero();
    for (var i = 0; i < 4; i++) {
      this.m[i*4+i] = 1;
    }
  };

  function loadProgram(gl) {
    var vertexShader = gl.createShader(gl.VERTEX_SHADER);
    gl.shaderSource(vertexShader, VERTEX_SHADER_SOURCE);
    gl.compileShader(vertexShader);

    var fragmentShader = gl.createShader(gl.FRAGMENT_SHADER);
    gl.shaderSource(fragmentShader, FRAGMENT_SHADER_SOURCE);
    gl.compileShader(fragmentShader);

    var program = gl.createProgram();
    gl.attachShader(program, vertexShader);
    gl.attachShader(program, fragmentShader);

    gl.linkProgram(program);
    // TODO(aa): Check for errors using getProgramiv and LINK_STATUS.

    gl.deleteShader(vertexShader);
    gl.deleteShader(fragmentShader);

    return program;
  }

  var vboVertices;
  var vboIndices;
  function generateCube(gl) {
    var numVertices = 24 * 3;
    var numIndices = 12 * 3;

    var cubeVertices = new Float32Array([
      -0.5, -0.5, -0.5,
      -0.5, -0.5,  0.5,
      0.5, -0.5,  0.5,
      0.5, -0.5, -0.5,
      -0.5,  0.5, -0.5,
      -0.5,  0.5,  0.5,
      0.5,  0.5,  0.5,
      0.5,  0.5, -0.5,
      -0.5, -0.5, -0.5,
      -0.5,  0.5, -0.5,
      0.5,  0.5, -0.5,
      0.5, -0.5, -0.5,
      -0.5, -0.5, 0.5,
      -0.5,  0.5, 0.5,
      0.5,  0.5, 0.5,
      0.5, -0.5, 0.5,
      -0.5, -0.5, -0.5,
      -0.5, -0.5,  0.5,
      -0.5,  0.5,  0.5,
      -0.5,  0.5, -0.5,
      0.5, -0.5, -0.5,
      0.5, -0.5,  0.5,
      0.5,  0.5,  0.5,
      0.5,  0.5, -0.5
    ]);

    var cubeIndices = new Uint16Array([
      0, 2, 1,
      0, 3, 2,
      4, 5, 6,
      4, 6, 7,
      8, 9, 10,
      8, 10, 11,
      12, 15, 14,
      12, 14, 13,
      16, 17, 18,
      16, 18, 19,
      20, 23, 22,
      20, 22, 21
    ]);

    // TODO(aa): The C++ program branches here on whether the pointer is
    // non-NULL.
    vboVertices = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, vboVertices);
    gl.bufferData(gl.ARRAY_BUFFER, cubeVertices, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ARRAY_BUFFER, 0);

    vboIndices = gl.createBuffer();
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, vboIndices);
    gl.bufferData(gl.ELEMENT_ARRAY_BUFFER, cubeIndices, gl.STATIC_DRAW);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, 0);

    return cubeIndices.length;
  }

  function drawCube(gl, width, height, program, positionLocation, mvpLocation,
                    numIndices, mvpMatrix) {
    gl.viewport(0, 0, width, height);
    gl.clear(gl.COLOR_BUFFER_BIT);
    gl.useProgram(program);
    gl.bindBuffer(gl.ARRAY_BUFFER, vboVertices);
    gl.bindBuffer(gl.ELEMENT_ARRAY_BUFFER, vboIndices);
    gl.vertexAttribPointer(positionLocation, 3, gl.FLOAT, false, 12, 0);
    gl.enableVertexAttribArray(positionLocation);
    gl.uniformMatrix4fv(mvpLocation, false, mvpMatrix.m);
    gl.drawElements(gl.TRIANGLES, numIndices, gl.UNSIGNED_SHORT, 0);
  }

  function SampleApp(shell) {
    this.shell_ = shell;

    var pipe = new core.createMessagePipe();
    new connector.Connection(pipe.handle0, NativeViewportClientImpl,
                             nativeViewport.NativeViewportProxy);
    this.shell_.connect('mojo:mojo_native_viewport_service', pipe.handle1);
  }
  // TODO(aa): It is a bummer to need this stub object in JavaScript. We should
  // have a 'client' object that contains both the sending and receiving bits of
  // the client side of the interface. Since JS is loosely typed, we do not need
  // a separate base class to inherit from to receive callbacks.
  SampleApp.prototype = Object.create(shell.ShellClientStub.prototype);


  function NativeViewportClientImpl(remote) {
    this.remote_ = remote;

    var pipe = core.createMessagePipe();
    new connector.Connection(pipe.handle0, GLES2ClientImpl, gles2.GLES2Proxy);

    this.remote_.open();
    this.remote_.createGLES2Context(pipe.handle1);
  }
  NativeViewportClientImpl.prototype =
      Object.create(nativeViewport.NativeViewportClientStub.prototype);

  NativeViewportClientImpl.prototype.onCreated = function() {
    console.log('NativeViewportClientImpl.prototype.OnCreated');
  };
  NativeViewportClientImpl.prototype.didOpen = function() {
    console.log('NativeViewportClientImpl.prototype.DidOpen');
  };


  function GLES2ClientImpl(remote) {
    this.remote_ = remote;
  }
  GLES2ClientImpl.prototype =
      Object.create(gles2.GLES2ClientStub.prototype);

  GLES2ClientImpl.prototype.didCreateContext = function(encoded,
                                                        width,
                                                        height) {
    var gl = new gljs.Context(encoded, width, height);
    var program = loadProgram(gl);
    var positionLocation = gl.getAttribLocation(program, 'a_position');
    var mvpLocation = gl.getUniformLocation(program, 'u_mvpMatrix');
    var numIndices = generateCube(gl);
    var mvpMatrix = new ESMatrix();

    gl.clearColor(0, 0, 0, 0);
    mvpMatrix.loadIdentity();
    drawCube(gl, width, height, program, positionLocation, mvpLocation,
             numIndices, mvpMatrix);
    gl.swapBuffers();
  };

  GLES2ClientImpl.prototype.contextLost = function() {
    console.log('GLES2ClientImpl.prototype.contextLost');
  };


  return function(handle) {
    new connector.Connection(handle, SampleApp, shell.ShellProxy);
  };
});
