// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var errorMsg = 'Not available for platform apps.';
var stub = function() { throw errorMsg; };

// Disable document.open|close|write.
document.open = stub;
document.close = stub;
document.write = stub;

// Disable history.
window.history = {
  open: stub,
  back: stub,
  forward: stub,
  go: stub,
  pushState: stub,
  replaceState: stub,
  get length() { throw errorMsg; },
  get state() { throw errorMsg; }
};

// Disable find.
window.find = stub;

// Disable modal dialogs.
window.alert = stub;
window.confirm = stub;
window.prompt = stub;

// Disable window.*bar.
var stubBar = { get visible() { throw errorMsg; } };
window.locationbar = stubBar;
window.menubar = stubBar;
window.personalbar = stubBar;
window.scrollbars = stubBar;
window.statusbar = stubBar;
window.toolbar = stubBar;
