// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

requireNative('runtime');

function set(message) {
  var errorObject = { 'message': message };
  if (chrome.extension)
    chrome.extension.lastError = errorObject;
  chrome.runtime.lastError = errorObject;
};

function clear() {
  if (chrome.extension)
    delete chrome.extension.lastError;
  delete chrome.runtime.lastError;
};

exports.clear = clear;
exports.set = set;
