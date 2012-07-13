// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function init() {
  window.addEventListener('load',
      windowStateManager.saveStates.bind(windowStateManager));
  window.addEventListener('focus',
      windowStateManager.saveStates.bind(windowStateManager));
  window.addEventListener('unload',
      windowStateManager.restoreStates.bind(windowStateManager));
}

document.addEventListener('DOMContentLoaded', init);
