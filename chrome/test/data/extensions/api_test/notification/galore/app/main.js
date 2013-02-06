// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var listener = Galore.controller.create.bind(Galore.controller);
  chrome.app.runtime.onLaunched.addListener(listener);
  chrome.app.runtime.onRestarted.addListener(listener);
}());
