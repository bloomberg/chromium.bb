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
  logEl.innerHTML += event.data + '<br>';
}
