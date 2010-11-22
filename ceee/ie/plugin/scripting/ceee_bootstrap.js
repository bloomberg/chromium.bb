// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview this file provides the bootstrap interface between the
 * Chrome event and extension bindings JavaScript files, and the CEEE
 * native IE interface, as well as initialization hooks for the native
 * interface.
 */


// Console is diverted to nativeContentScriptApi.Log method.
var console = console || {};

// Any function declared native in the Chrome extension bindings files
// is diverted to the ceee namespace to allow the IE JS engine
// to grok the code.
var ceee = ceee || {};

(function () {
// Keep a reference to the global environment in
// effect during boostrap script parsing.
var global = this;

var chromeHidden = {};
var nativeContentScriptApi = null;

// Supply a JSON implementation by leeching off closure.
global.JSON = goog.json;
global.JSON.stringify = JSON.serialize;

ceee.AttachEvent = function (eventName) {
  nativeContentScriptApi.AttachEvent(eventName);
};

ceee.DetachEvent = function (eventName) {
  nativeContentScriptApi.DetachEvent(eventName);
};

ceee.OpenChannelToExtension = function (sourceId, targetId, name) {
  return nativeContentScriptApi.OpenChannelToExtension(sourceId,
                                                       targetId,
                                                       name);
};

ceee.CloseChannel = function (portId) {
  return nativeContentScriptApi.CloseChannel(portId);
};

ceee.PortAddRef = function (portId) {
  return nativeContentScriptApi.PortAddRef(portId);
};

ceee.PortRelease = function (portId) {
  return nativeContentScriptApi.PortRelease(portId);
};

ceee.PostMessage = function (portId, msg) {
  return nativeContentScriptApi.PostMessage(portId, msg);
};

ceee.GetChromeHidden = function () {
  return chromeHidden;
};

// This function is invoked from the native CEEE implementation by name
// to pass in the native CEEE interface implementation at the start of
// script engine initialization. This allows us to provide logging
// and any other required or convenient services during the initialization
// of other boostrap scripts.
ceee.startInit_ = function (nativenativeContentScriptApi, extensionId) {
  nativeContentScriptApi = nativenativeContentScriptApi;
};

// Last uninitialization callback.
ceee.onUnload_ = function () {
  // Dispatch the onUnload event.
  chromeHidden.dispatchOnUnload();

  // Release the native API as very last act.
  nativeContentScriptApi = null;
};

// This function is invoked from the native CEEE implementation by name
// to pass in the extension ID, and to allow any final initialization of
// the script environment before the content scripts themselves are loaded.
ceee.endInit_ = function (nativenativeContentScriptApi, extensionId) {
  chrome.initExtension(extensionId);

  // Provide the native implementation with the the the
  // event notification dispatchers.
  nativeContentScriptApi.onLoad = chromeHidden.dispatchOnLoad;
  nativeContentScriptApi.onUnload = ceee.onUnload_;

  // And the port notification dispatchers.
  // function(portId, channelName, tab, extensionId)
  nativeContentScriptApi.onPortConnect = chromeHidden.Port.dispatchOnConnect;

  // function(portId)
  nativeContentScriptApi.onPortDisconnect =
      chromeHidden.Port.dispatchOnDisconnect;
  // function(msg, portId)
  nativeContentScriptApi.onPortMessage =
      chromeHidden.Port.dispatchOnMessage;

  // TODO(siggi@chromium.org): If there is a different global
  //    environment at this point (i.e. we have cloned the scripting
  //    engine for a new window) this is where we can restore goog,
  //    JSON and chrome.

  // Delete the ceee namespace from globals.
  delete ceee;
};

console.log = console.log || function (msg) {
  if (nativeContentScriptApi)
    nativeContentScriptApi.Log("info", msg);
};

console.error = console.error || function (msg) {
  if (nativeContentScriptApi)
    nativeContentScriptApi.Log("error", msg);
}

// Provide an indexOf member for arrays if it's not already there
// to satisfy the Chrome extension bindings expectations.
if (!Array.prototype.indexOf) {
  Array.prototype.indexOf = function(elt /*, from*/) {
    var len = this.length >>> 0;

    var from = Number(arguments[1]) || 0;
    from = (from < 0) ? Math.ceil(from) : Math.floor(from);
    if (from < 0)
      from += len;

    for (; from < len; from++) {
      if (from in this && this[from] === elt)
        return from;
    }
    return -1;
  }
};

})();
