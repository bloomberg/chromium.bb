// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the devtools API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('devtools', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('getTabEvents', function(tabId) {
    var tabIdProxy = {};
    var functions = ['onPageEvent', 'onTabClose'];
    functions.forEach(function(name) {
      // Event disambiguation is handled by name munging.  See
      // chrome/browser/extensions/extension_devtools_events.h for the C++
      // equivalent of this logic.
      tabIdProxy[name] = new chrome.Event('devtools.' + tabId + '.' + name);
    });
    return tabIdProxy;
  });
});
