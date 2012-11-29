// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

function initialize() {
  $('bypass-block-button').onclick = function(event) {
    sendCommand('preview');
  }
  $('back-button').onclick = function(event) {
    sendCommand('back');
  };
  $('content-packs-section-button').onclick = function(event) {
    sendCommand('ntp');
  };
}

document.addEventListener('DOMContentLoaded', initialize);
