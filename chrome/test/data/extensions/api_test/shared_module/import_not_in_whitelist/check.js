// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

if (typeof fail_exported != 'undefined')
  chrome.test.notifyFail('fail.js was unintentionally exported.');

if (typeof pass_exported == 'undefined')
  chrome.test.notifyFail('pass.js was not exported correctly.');

chrome.test.notifyPass();
