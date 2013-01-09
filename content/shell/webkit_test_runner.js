// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var testRunner = testRunner || {};

(function() {
  native function CloseWebInspector();
  native function Display();
  native function DumpEditingCallbacks();
  native function DumpFrameLoadCallbacks();
  native function DumpUserGestureInFrameLoadCallbacks();
  native function SetStopProvisionalFrameLoads();
  native function DumpTitleChanges();
  native function EvaluateInWebInspector();
  native function ExecCommand();
  native function GetWorkerThreadCount();
  native function NotifyDone();
  native function OverridePreference();
  native function SetCanOpenWindows();
  native function SetDumpAsText();
  native function SetDumpChildFramesAsText();
  native function SetPrinting();
  native function SetShouldStayOnPageAfterHandlingBeforeUnload();
  native function SetWaitUntilDone();
  native function SetXSSAuditorEnabled();
  native function ShowWebInspector();

  native function GetGlobalFlag();
  native function SetGlobalFlag();

  native function NotImplemented();

  var DefaultHandler = function(name) {
    var handler = {
      get: function(receiver, property) {
        if (property === "splice")
          return undefined;
        if (property === "__proto__")
          return {}
        if (property === "toString")
          return function() { return "[object Object]"; };
        if (property === "constructor" || property === "valueOf")
          return function() { return {}; };
        NotImplemented(name, property);
        return function() {}
      },
      getOwnPropertyNames: function() {
        return [];
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
    Object.defineProperty(this, "execCommand", {value: ExecCommand});
    Object.defineProperty(this,
                          "overridePreference",
                          {value: OverridePreference});
    Object.defineProperty(this,
                          "dumpEditingCallbacks",
                          {value: DumpEditingCallbacks});
    Object.defineProperty(this,
                          "dumpFrameLoadCallbacks",
                          {value: DumpFrameLoadCallbacks});
    Object.defineProperty(this,
                          "dumpUserGestureInFrameLoadCallbacks",
                          {value: DumpUserGestureInFrameLoadCallbacks});
    Object.defineProperty(this,
                          "setStopProvisionalFrameLoads",
                          {value: SetStopProvisionalFrameLoads});
    Object.defineProperty(this,
                          "dumpTitleChanges",
                          {value: DumpTitleChanges});

    Object.defineProperty(this,
                          "globalFlag",
                          {
                            get: GetGlobalFlag,
                            set: SetGlobalFlag,
                            writeable: true,
                            configurable: true,
                            enumerable: true
                          });
    Object.defineProperty(this,
                          "platformName",
                          {
                            value: "chromium",
                            writeable: true,
                            configurable: true,
                            enumerable: true
                          });
    Object.defineProperty(this,
                          "workerThreadCount",
                          {get: GetWorkerThreadCount});

    var stubs = [
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
        "applicationCacheDiskUsageForOrigin",
        "abortModal"
    ];
    for (var idx in stubs) {
      Object.defineProperty(
          this, stubs[idx], {value: function() { return null; }});
    }
  }
  TestRunner.prototype = DefaultHandler("testRunner");
  testRunner = new TestRunner();
})();
