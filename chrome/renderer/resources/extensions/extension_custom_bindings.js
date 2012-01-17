// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the extension API.

(function() {

native function GetChromeHidden();
native function GetExtensionViews();

GetChromeHidden().registerCustomHook('extension', function(bindingsAPI) {
  // getTabContentses is retained for backwards compatibility.
  // See http://crbug.com/21433
  chrome.extension.getTabContentses = chrome.extension.getExtensionTabs;

  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest("extension.getViews", function(properties) {
    var windowId = chrome.windows.WINDOW_ID_NONE;
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
      windowId = chrome.windows.WINDOW_ID_NONE;
    return GetExtensionViews(windowId, "TAB");
  });
});

})();
