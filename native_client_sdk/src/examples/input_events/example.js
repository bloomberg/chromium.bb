// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Called by the common.js module.
function moduleDidLoad() {
  common.naclModule.style.backgroundColor = 'gray';
}

// Called by the common.js module.
function attachListeners() {
  document.getElementById('killButton').addEventListener('click', cancelQueue);
}

// Called by the common.js module.
function handleMessage(message) {
  common.logMessage(message.data);
}

function cancelQueue() {
  if (common.naclModule == null) {
    console.log('Module is not loaded.');
    return;
  }
  common.naclModule.postMessage('CANCEL');
}
