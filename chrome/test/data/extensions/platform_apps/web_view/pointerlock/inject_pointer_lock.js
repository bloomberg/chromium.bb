// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var embedder = null;
window.addEventListener('message', function(e) {
  var data = JSON.parse(e.data);
  if (data[0] == 'connect') {
    embedder = e.source;
    var msg = ['connected'];
    embedder.postMessage(JSON.stringify(msg), '*');
    return;
  }

  if (data == 'start-pointerlock') {
    document.body.webkitRequestPointerLock();
    return;
  }
});

document.addEventListener('webkitpointerlockchange', function(e) {
  if (!!document.webkitPointerLockElement) {
    var msg = ['acquired-pointerlock'];
    embedder.postMessage(JSON.stringify(msg), '*');
    return;
  }
  var msg = ['lost-pointerlock'];
  embedder.postMessage(JSON.stringify(msg), '*');
});
