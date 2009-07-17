// Copyright (c) 2009 The chrome Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// -----------------------------------------------------------------------------
// NOTE: If you change this file you need to touch renderer_resources.grd to
// have your change take effect.
// -----------------------------------------------------------------------------

// This script contains privileged chrome extension related javascript APIs.
// It is loaded by pages whose URL has the chrome-extension protocol.

var chrome = chrome || {};
(function() {
  native function StartRequest();
  native function GetCurrentPageActions();
  native function GetViews();
  native function GetChromeHidden();
  native function GetNextRequestId();

  if (!chrome)
    chrome = {};

  var chromeHidden = GetChromeHidden();

  // Validate arguments.
  function validate(args, schemas) {
    if (args.length > schemas.length)
      throw new Error("Too many arguments.");

    for (var i = 0; i < schemas.length; i++) {
      if (i in args && args[i] !== null && args[i] !== undefined) {
        var validator = new chrome.JSONSchemaValidator();
        validator.validate(args[i], schemas[i]);
        if (validator.errors.length == 0)
          continue;

        var message = "Invalid value for argument " + i + ". ";
        for (var i = 0, err; err = validator.errors[i]; i++) {
          if (err.path) {
            message += "Property '" + err.path + "': ";
          }
          message += err.message;
          message = message.substring(0, message.length - 1);
          message += ", ";
        }
        message = message.substring(0, message.length - 2);
        message += ".";

        throw new Error(message);
      } else if (!schemas[i].optional) {
        throw new Error("Parameter " + i + " is required.");
      }
    }
  }

  // Callback handling.
  var callbacks = [];
  chromeHidden.handleResponse = function(requestId, name,
                                         success, response, error) {
    try {
      if (!success) {
        if (!error)
          error = "Unknown error."
        console.error("Error during " + name + ": " + error);
        return;
      }

      if (callbacks[requestId]) {
        if (response) {
          callbacks[requestId](JSON.parse(response));
        } else {
          callbacks[requestId]();
        }
      }
    } finally {
      delete callbacks[requestId];
    }
  };

  // Send an API request and optionally register a callback.
  function sendRequest(functionName, args, callback) {
    // JSON.stringify doesn't support a root object which is undefined.
    if (args === undefined)
      args = null;
    var sargs = JSON.stringify(args);
    var requestId = GetNextRequestId();
    var hasCallback = false;
    if (callback) {
      hasCallback = true;
      callbacks[requestId] = callback;
    }
    StartRequest(functionName, sargs, requestId, hasCallback);
  }

  //----------------------------------------------------------------------------

  // Windows.
  chrome.windows = {};

  chrome.windows.get = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("windows.get", windowId, callback);
  };

  chrome.windows.get.params = [
    chrome.types.pInt,
    chrome.types.fun
  ];

  chrome.windows.getCurrent = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("windows.getCurrent", null, callback);
  };

  chrome.windows.getCurrent.params = [
    chrome.types.fun
  ];

  chrome.windows.getLastFocused = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("windows.getLastFocused", null, callback);
  };

  chrome.windows.getLastFocused.params = [
    chrome.types.fun
  ];

  chrome.windows.getAll = function(populate, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("windows.getAll", populate, callback);
  };

  chrome.windows.getAll.params = [
    chrome.types.optBool,
    chrome.types.fun
  ];

  chrome.windows.create = function(createData, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("windows.create", createData, callback);
  };
  chrome.windows.create.params = [
    {
      type: "object",
      properties: {
        url: chrome.types.optStr,
        left: chrome.types.optInt,
        top: chrome.types.optInt,
        width: chrome.types.optPInt,
        height: chrome.types.optPInt
      },
      optional: true
    },
    chrome.types.optFun
  ];

  chrome.windows.update = function(windowId, updateData, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("windows.update", [windowId, updateData], callback);
  };
  chrome.windows.update.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        left: chrome.types.optInt,
        top: chrome.types.optInt,
        width: chrome.types.optPInt,
        height: chrome.types.optPInt
      },
    },
    chrome.types.optFun
  ];

  chrome.windows.remove = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("windows.remove", windowId, callback);
  };

  chrome.windows.remove.params = [
    chrome.types.pInt,
    chrome.types.optFun
  ];

  // sends (windowId).
  // *WILL* be followed by tab-attached AND then tab-selection-changed.
  chrome.windows.onCreated = new chrome.Event("windows.onCreated");

  // sends (windowId).
  // *WILL* be preceded by sequences of tab-removed AND then
  // tab-selection-changed -- one for each tab that was contained in the window
  // that closed
  chrome.windows.onRemoved = new chrome.Event("windows.onRemoved");

  // sends (windowId).
  chrome.windows.onFocusChanged =
        new chrome.Event("windows.onFocusChanged");

  //----------------------------------------------------------------------------

  // Tabs
  chrome.tabs = {};

  chrome.tabs.get = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.get", tabId, callback);
  };

  chrome.tabs.get.params = [
    chrome.types.pInt,
    chrome.types.fun
  ];

  chrome.tabs.getSelected = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.getSelected", windowId, callback);
  };

  chrome.tabs.getSelected.params = [
    chrome.types.optPInt,
    chrome.types.fun
  ];

  chrome.tabs.getAllInWindow = function(windowId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.getAllInWindow", windowId, callback);
  };

  chrome.tabs.getAllInWindow.params = [
    chrome.types.optPInt,
    chrome.types.fun
  ];

  chrome.tabs.create = function(tab, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.create", tab, callback);
  };

  chrome.tabs.create.params = [
    {
      type: "object",
      properties: {
        windowId: chrome.types.optPInt,
        index: chrome.types.optPInt,
        url: chrome.types.optStr,
        selected: chrome.types.optBool
      }
    },
    chrome.types.optFun
  ];

  chrome.tabs.update = function(tabId, updates, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.update", [tabId, updates], callback);
  };

  chrome.tabs.update.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        url: chrome.types.optStr,
        selected: chrome.types.optBool
      }
    },
    chrome.types.optFun
  ];

  chrome.tabs.move = function(tabId, moveProps, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.move", [tabId, moveProps], callback);
  };

  chrome.tabs.move.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        windowId: chrome.types.optPInt,
        index: chrome.types.pInt
      }
    },
    chrome.types.optFun
  ];

  chrome.tabs.remove = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.remove", tabId, callback);
  };

  chrome.tabs.remove.params = [
    chrome.types.pInt,
    chrome.types.optFun
  ];

  chrome.tabs.detectLanguage = function(tabId, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("tabs.detectLanguage", tabId, callback);
  };

  chrome.tabs.detectLanguage.params = [
    chrome.types.optPInt,
    chrome.types.optFun
  ];

  // Sends ({Tab}).
  // Will *NOT* be followed by tab-attached - it is implied.
  // *MAY* be followed by tab-selection-changed.
  chrome.tabs.onCreated = new chrome.Event("tabs.onCreated");

  // Sends (tabId, {ChangedProps}).
  chrome.tabs.onUpdated = new chrome.Event("tabs.onUpdated");

  // Sends (tabId, {windowId, fromIndex, toIndex}).
  // Tabs can only "move" within a window.
  chrome.tabs.onMoved = new chrome.Event("tabs.onMoved");

  // Sends (tabId, {windowId}).
  chrome.tabs.onSelectionChanged =
       new chrome.Event("tabs.onSelectionChanged");

  // Sends (tabId, {newWindowId, newPosition}).
  // *MAY* be followed by tab-selection-changed.
  chrome.tabs.onAttached = new chrome.Event("tabs.onAttached");

  // Sends (tabId, {oldWindowId, oldPosition}).
  // *WILL* be followed by tab-selection-changed.
  chrome.tabs.onDetached = new chrome.Event("tabs.onDetached");

  // Sends (tabId).
  // *WILL* be followed by tab-selection-changed.
  // Will *NOT* be followed or preceded by tab-detached.
  chrome.tabs.onRemoved = new chrome.Event("tabs.onRemoved");

  //----------------------------------------------------------------------------

  // PageActions.
  chrome.pageActions = {};

  chrome.pageActions.enableForTab = function(pageActionId, action) {
    validate(arguments, arguments.callee.params);
    sendRequest("pageActions.enableForTab", [pageActionId, action]);
  }

  chrome.pageActions.enableForTab.params = [
    chrome.types.str,
    {
      type: "object",
      properties: {
        tabId: chrome.types.pInt,
        url: chrome.types.str,
        title: chrome.types.optStr,
        iconId: chrome.types.optPInt,
      },
      optional: false
    }
  ];

  chrome.pageActions.disableForTab = function(pageActionId, action) {
    validate(arguments, arguments.callee.params);
    sendRequest("pageActions.disableForTab", [pageActionId, action]);
  }

  chrome.pageActions.disableForTab.params = [
    chrome.types.str,
    {
      type: "object",
      properties: {
        tabId: chrome.types.pInt,
        url: chrome.types.str
      },
      optional: false
    }
  ];

  // Page action events send (pageActionId, {tabId, tabUrl}).
  function setupPageActionEvents(extensionId) {
    var pageActions = GetCurrentPageActions();
    var eventName = "";
    for (var i = 0; i < pageActions.length; ++i) {
      eventName = extensionId + "/" + pageActions[i];
      // Setup events for each extension_id/page_action_id string we find.
      chrome.pageActions[pageActions[i]] = new chrome.Event(eventName);
    }
  }

  //----------------------------------------------------------------------------
  // Bookmarks
  chrome.bookmarks = {};

  chrome.bookmarks.get = function(ids, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.get", ids, callback);
  };

  chrome.bookmarks.get.params = [
    chrome.types.singleOrListOf(chrome.types.pInt),
    chrome.types.fun
  ];

  chrome.bookmarks.getChildren = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.getChildren", id, callback);
  };

  chrome.bookmarks.getChildren.params = [
    chrome.types.pInt,
    chrome.types.fun
  ];

  chrome.bookmarks.getTree = function(callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.getTree", null, callback);
  };

  // TODO(erikkay): allow it to take an optional id as a starting point
  // BUG=13727
  chrome.bookmarks.getTree.params = [
    chrome.types.fun
  ];

  chrome.bookmarks.search = function(query, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.search", query, callback);
  };

  chrome.bookmarks.search.params = [
    chrome.types.str,
    chrome.types.fun
  ];

  chrome.bookmarks.remove = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.remove", [id, false], callback);
  };

  chrome.bookmarks.remove.params = [
    chrome.types.singleOrListOf(chrome.types.pInt),
    chrome.types.optFun
  ];

  chrome.bookmarks.removeTree = function(id, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.removeTree", [id, true], callback);
  };

  chrome.bookmarks.removeTree.params = [
    chrome.types.pInt,
    chrome.types.optFun
  ];

  chrome.bookmarks.create = function(bookmark, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.create", bookmark, callback);
  };

  chrome.bookmarks.create.params = [
    {
      type: "object",
      properties: {
        parentId: chrome.types.optPInt,
        index: chrome.types.optPInt,
        title: chrome.types.optStr,
        url: chrome.types.optStr,
      }
    },
    chrome.types.optFun
  ];

  chrome.bookmarks.move = function(id, destination, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.move", [id, destination], callback);
  };

  chrome.bookmarks.move.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        parentId: chrome.types.optPInt,
        index: chrome.types.optPInt
      }
    },
    chrome.types.optFun
  ];

  chrome.bookmarks.update = function(id, changes, callback) {
    validate(arguments, arguments.callee.params);
    sendRequest("bookmarks.update", [id, changes], callback);
  };

  chrome.bookmarks.update.params = [
    chrome.types.pInt,
    {
      type: "object",
      properties: {
        title: chrome.types.optStr
      }
    },
    chrome.types.optFun
  ];

  // bookmark events

  // Sends (id, {title, url, parentId, index, dateAdded, dateGroupModified})
  // date values are milliseconds since the UTC epoch.
  chrome.bookmarks.onAdded = new chrome.Event("bookmarks.onAdded");

  // Sends (id, {parentId, index})
  chrome.bookmarks.onRemoved = new chrome.Event("bookmarks.onRemoved");

  // Sends (id, object) where object has list of properties that have changed.
  // Currently, this only ever includes 'title'.
  chrome.bookmarks.onChanged = new chrome.Event("bookmarks.onChanged");

  // Sends (id, {parentId, index, oldParentId, oldIndex})
  chrome.bookmarks.onMoved = new chrome.Event("bookmarks.onMoved");

  // Sends (id, [childrenIds])
  chrome.bookmarks.onChildrenReordered =
      new chrome.Event("bookmarks.onChildrenReordered");


  //----------------------------------------------------------------------------

  // Self.
  chrome.self = chrome.self || {};

  chromeHidden.onLoad.addListener(function (extensionId) {
    chrome.extension = new chrome.Extension(extensionId);
    // TODO(mpcomplete): self.onConnect is deprecated.  Remove it at 1.0.
    // http://code.google.com/p/chromium/issues/detail?id=16356
    chrome.self.onConnect = new chrome.Event("channel-connect:" + extensionId);

    setupPageActionEvents(extensionId);
  });

  chrome.self.getViews = function() {
    return GetViews();
  }
})();
