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
});

window.addEventListener('focus', function(e) {
  var msg = ['focused'];
  embedder.postMessage(JSON.stringify(msg), '*');
});

window.addEventListener('blur', function(e) {
  var msg = ['blurred'];
  embedder.postMessage(JSON.stringify(msg), '*');
});
