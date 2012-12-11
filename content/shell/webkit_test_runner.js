// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testRunner = testRunner || {};

(function() {
  native function CloseWebInspector();
  native function Display();
  native function EvaluateInWebInspector();
  native function GetWorkerThreadCount();
  native function NotifyDone();
  native function SetCanOpenWindows();
  native function SetDumpAsText();
  native function SetDumpChildFramesAsText();
  native function SetPrinting();
  native function SetShouldStayOnPageAfterHandlingBeforeUnload();
  native function SetWaitUntilDone();
  native function SetXSSAuditorEnabled();
  native function ShowWebInspector();

  native function NotImplemented();

  var DefaultHandler = function(name) {
    var handler = {
      get: function(receiver, property) {
        NotImplemented(name, property);
        return function() {}
      },
      getPropertyDescriptor: function(property) {
        NotImplemented(name, property);
        return undefined;
      }
    }
    return Proxy.create(handler);
  }

  var TestRunner = function() {
    Object.defineProperty(this, "display", {value: Display});
    Object.defineProperty(this,
                          "workerThreadCount",
                          {value: GetWorkerThreadCount});
    Object.defineProperty(this, "notifyDone", {value: NotifyDone});
    Object.defineProperty(this, "dumpAsText", {value: SetDumpAsText});
    Object.defineProperty(this,
                          "dumpChildFramesAsText",
                          {value: SetDumpChildFramesAsText});
    Object.defineProperty(this,
                          "setCanOpenWindows",
                          {value: SetCanOpenWindows});
    Object.defineProperty(this, "setPrinting", {value: SetPrinting});
    Object.defineProperty(
        this,
        "setShouldStayOnPageAfterHandlingBeforeUnload",
        {value: SetShouldStayOnPageAfterHandlingBeforeUnload});
    Object.defineProperty(this,
                          "setXSSAuditorEnabled",
                          {value: SetXSSAuditorEnabled});
    Object.defineProperty(this, "waitUntilDone", {value: SetWaitUntilDone});
    Object.defineProperty(this, "showWebInspector", {value: ShowWebInspector});
    Object.defineProperty(this,
                          "closeWebInspector",
                          {value: CloseWebInspector});
    Object.defineProperty(this,
                          "evaluateInWebInspector",
                          {value: EvaluateInWebInspector});

    var stubs = [
        "overridePreference",  // not really a stub, but required to pass
                               // content_browsertests for now.
        "dumpDatabaseCallbacks",
        "denyWebNotificationPermission",
        "removeAllWebNotificationPermissions",
        "simulateWebNotificationClick",
        "setIconDatabaseEnabled",
        "setScrollbarPolicy",
        "clearAllApplicationCaches",
        "clearApplicationCacheForOrigin",
        "clearBackForwardList",
        "keepWebHistory",
        "setApplicationCacheOriginQuota",
        "setCallCloseOnWebViews",
        "setMainFrameIsFirstResponder",
        "setPrivateBrowsingEnabled",
        "setUseDashboardCompatibilityMode",
        "deleteAllLocalStorage",
        "localStorageDiskUsageForOrigin",
        "originsWithLocalStorage",
        "deleteLocalStorageForOrigin",
        "observeStorageTrackerNotifications",
        "syncLocalStorage",
        "addDisallowedURL",
        "applicationCacheDiskUsageForOrigin"
    ];
    for (var stub in stubs) {
      Object.defineProperty(this, stub, {value: function() { return null; }});
    }
  }
  TestRunner.prototype = DefaultHandler("testRunner");
  testRunner = new TestRunner();
})();
