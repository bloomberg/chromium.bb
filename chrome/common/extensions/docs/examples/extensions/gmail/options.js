// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var customDomainsTextbox;
var saveButton;

function init() {
  customDomainsTextbox = document.getElementById("custom-domain");
  saveButton = document.getElementById("save-button");

  customDomainsTextbox.value = localStorage.customDomain || "";
  markClean();
}

function save() {
  localStorage.customDomain = customDomainsTextbox.value;
  markClean();

  chrome.extension.getBackgroundPage().init();
}

function markDirty() {
  saveButton.disabled = false;
}

function markClean() {
  saveButton.disabled = true;
}

document.addEventListener('DOMContentLoaded', function () {
  init();
  document.querySelector('#cancel-button').addEventListener('DOMContentLoaded',
                                                            init);
  document.querySelector('#save-button').addEventListener('DOMContentLoaded',
                                                          save);
});
