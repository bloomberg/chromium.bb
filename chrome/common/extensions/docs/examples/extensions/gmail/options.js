// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var customDomainsTextbox;
var saveButton;
var cancelButton;

function init() {
  customDomainsTextbox = document.getElementById("custom-domain");
  saveButton = document.getElementById("save-button");
  cancelButton = document.getElementById("cancel-button");

  customDomainsTextbox.value = localStorage.customDomain || "";
  markClean();
}

function getBackgroundPage(callback) {
  if (chrome.runtime) {
    chrome.runtime.getBackgroundPage(callback);
  } else {
    callback(chrome.extension.getBackgroundPage());
  }
}

function save() {
  localStorage.customDomain =
      customDomainsTextbox.value.replace(/^\/?(.*?)\/?$/, '$1');
  markClean();
  getBackgroundPage(function(backgroundPage) {
    backgroundPage.startRequest({
      scheduleRequest:false,
      showLoadingAnimation:true
    });
  });
}

function markDirty() {
  saveButton.disabled = false;
}

function markClean() {
  saveButton.disabled = true;
}

document.addEventListener('DOMContentLoaded', function () {
  init();
  saveButton.addEventListener('click', save);
  cancelButton.addEventListener('click', init);
  customDomainsTextbox.addEventListener('input', markDirty);
});
