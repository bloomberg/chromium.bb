// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Called by the common.js module.
function moduleDidLoad() {
  // The module is not hidden by default so we can easily see if the plugin
  // failed to load.
  common.hideModule();
  common.naclModule.postMessage('RunGTest');
}

function handleMessage(event) {
  var logEl = document.getElementById('log');
  var msg = event.data;

  // Perform some basic escaping.
  msg = msg.replace(/&/g, '&amp;')
           .replace(/</g, '&lt;')
           .replace(/>/g, '&gt;')
           .replace(/"/g, '&quot;')
           .replace(/'/g, '&apos;');

  logEl.innerHTML += msg + '\n';
}
