// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var errorMsg = 'Not available for platform apps.';
var stub = function() { throw errorMsg; };

// Disable document.open|close|write.
HTMLDocument.prototype.open = stub;
HTMLDocument.prototype.close = stub;
HTMLDocument.prototype.write = stub;

// Disable history.
window.history = {
  back: stub,
  forward: stub,
  go: stub,
  pushState: stub,
  replaceState: stub,
  get length() { throw errorMsg; },
  get state() { throw errorMsg; }
};

// Disable find.
Window.prototype.find = stub;

// Disable modal dialogs. Shell windows disable these anyway, but it's nice to
// warn.
Window.prototype.alert = stub;
Window.prototype.confirm = stub;
Window.prototype.prompt = stub;

// Disable window.*bar.
var stubBar = { get visible() { throw errorMsg; } };
window.locationbar = stubBar;
window.menubar = stubBar;
window.personalbar = stubBar;
window.scrollbars = stubBar;
window.statusbar = stubBar;
window.toolbar = stubBar;

// Disable onunload, onbeforeunload.
Window.prototype.__defineSetter__('onbeforeunload', stub);
Window.prototype.__defineSetter__('onunload', stub);
var windowAddEventListener = Window.prototype.addEventListener;
Window.prototype.addEventListener = function(type) {
  if (type === 'unload' || type === 'beforeunload')
    stub();
  else
    return windowAddEventListener.apply(window, arguments);
};
