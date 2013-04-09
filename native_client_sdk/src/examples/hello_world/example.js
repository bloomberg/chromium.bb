// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Once we load, hide the plugin
function moduleDidLoad() {
  common.hideModule();
}

// Called by the common.js module.
function handleMessage(message) {
  common.logMessage(message.data);
}
