// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if (mojo && mojo.internal) {
  throw new Error('The Mojo bindings library has been initialized.');
}

var mojo = mojo || {};
mojo.internal = {};
mojo.internal.global = this;

(function() {
  var internal = mojo.internal;

  function exposeNamespace(namespace) {
    var current = internal.global;
    var parts = namespace.split('.');

    for (var part; parts.length && (part = parts.shift());) {
      if (!current[part]) {
        current[part] = {};
      }
      current = current[part];
    }

    return current;
  }

  internal.exposeNamespace = exposeNamespace;
})();
