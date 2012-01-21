// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the extension API.

(function() {

native function GetChromeHidden();
native function GetExtensionViews();

// This should match chrome.windows.WINDOW_ID_NONE.
//
// We can't use chrome.windows.WINDOW_ID_NONE directly because the
// chrome.windows API won't exist unless this extension has permission for it;
// which may not be the case.
var WINDOW_ID_NONE = -1;

GetChromeHidden().registerCustomHook('extension', function(bindingsAPI) {
  // getTabContentses is retained for backwards compatibility.
  // See http://crbug.com/21433
  chrome.extension.getTabContentses = chrome.extension.getExtensionTabs;

  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest("extension.getViews", function(properties) {
    var windowId = WINDOW_ID_NONE;
    var type = "ALL";
    if (typeof(properties) != "undefined") {
      if (typeof(properties.type) != "undefined") {
        type = properties.type;
      }
      if (typeof(properties.windowId) != "undefined") {
        windowId = properties.windowId;
      }
    }
    return GetExtensionViews(windowId, type) || null;
  });

  apiFunctions.setHandleRequest("extension.getBackgroundPage", function() {
    return GetExtensionViews(-1, "BACKGROUND")[0] || null;
  });

  apiFunctions.setHandleRequest("extension.getExtensionTabs",
      function(windowId) {
    if (typeof(windowId) == "undefined")
      windowId = WINDOW_ID_NONE;
    return GetExtensionViews(windowId, "TAB");
  });
});

})();
