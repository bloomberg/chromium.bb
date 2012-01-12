// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the webRequest API.

(function() {

native function GetChromeHidden();
native function GetUniqueSubEventName(eventName);

var chromeHidden = GetChromeHidden();

// WebRequestEvent object. This is used for special webRequest events with
// extra parameters. Each invocation of addListener creates a new named
// sub-event. That sub-event is associated with the extra parameters in the
// browser process, so that only it is dispatched when the main event occurs
// matching the extra parameters.
//
// Example:
//   chrome.webRequest.onBeforeRequest.addListener(
//       callback, {urls: "http://*.google.com/*"});
//   ^ callback will only be called for onBeforeRequests matching the filter.
function WebRequestEvent(eventName, opt_argSchemas, opt_extraArgSchemas) {
  if (typeof eventName != "string")
    throw new Error("chrome.WebRequestEvent requires an event name.");

  this.eventName_ = eventName;
  this.argSchemas_ = opt_argSchemas;
  this.extraArgSchemas_ = opt_extraArgSchemas;
  this.subEvents_ = [];
};

// Test if the given callback is registered for this event.
WebRequestEvent.prototype.hasListener = function(cb) {
  return this.findListener_(cb) > -1;
};

// Test if any callbacks are registered fur thus event.
WebRequestEvent.prototype.hasListeners = function() {
  return this.subEvents_.length > 0;
};

// Registers a callback to be called when this event is dispatched. If
// opt_filter is specified, then the callback is only called for events that
// match the given filters. If opt_extraInfo is specified, the given optional
// info is sent to the callback.
WebRequestEvent.prototype.addListener =
    function(cb, opt_filter, opt_extraInfo) {
  var subEventName = GetUniqueSubEventName(this.eventName_);
  // Note: this could fail to validate, in which case we would not add the
  // subEvent listener.
  chromeHidden.validate(Array.prototype.slice.call(arguments, 1),
                        this.extraArgSchemas_);
  chrome.webRequest.addEventListener(
      cb, opt_filter, opt_extraInfo, this.eventName_, subEventName);

  var subEvent = new chrome.Event(subEventName, this.argSchemas_);
  var subEventCallback = cb;
  if (opt_extraInfo && opt_extraInfo.indexOf("blocking") >= 0) {
    var eventName = this.eventName_;
    subEventCallback = function() {
      var requestId = arguments[0].requestId;
      try {
        var result = cb.apply(null, arguments);
        chrome.webRequest.eventHandled(
            eventName, subEventName, requestId, result);
      } catch (e) {
        chrome.webRequest.eventHandled(
            eventName, subEventName, requestId);
        throw e;
      }
    };
  } else if (opt_extraInfo && opt_extraInfo.indexOf("asyncBlocking") >= 0) {
    var eventName = this.eventName_;
    subEventCallback = function() {
      var details = arguments[0];
      var requestId = details.requestId;
      var handledCallback = function(response) {
        chrome.webRequest.eventHandled(
            eventName, subEventName, requestId, response);
      };
      cb.apply(null, [details, handledCallback]);
    };
  }
  this.subEvents_.push(
      {subEvent: subEvent, callback: cb, subEventCallback: subEventCallback});
  subEvent.addListener(subEventCallback);
};

// Unregisters a callback.
WebRequestEvent.prototype.removeListener = function(cb) {
  var idx;
  while ((idx = this.findListener_(cb)) >= 0) {
    var e = this.subEvents_[idx];
    e.subEvent.removeListener(e.subEventCallback);
    if (e.subEvent.hasListeners()) {
      console.error(
          "Internal error: webRequest subEvent has orphaned listeners.");
    }
    this.subEvents_.splice(idx, 1);
  }
};

WebRequestEvent.prototype.findListener_ = function(cb) {
  for (var i in this.subEvents_) {
    var e = this.subEvents_[i];
    if (e.callback === cb) {
      if (e.subEvent.findListener_(e.subEventCallback) > -1)
        return i;
      console.error("Internal error: webRequest subEvent has no callback.");
    }
  }

  return -1;
};

chromeHidden.registerCustomEvent('webRequest', WebRequestEvent);

chromeHidden.registerCustomHook('webRequest', function(api) {
  var apiFunctions = api.apiFunctions;
  var sendRequest = api.sendRequest;

  apiFunctions.setHandleRequest("webRequest.addEventListener",
      function() {
    var args = Array.prototype.slice.call(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });

  apiFunctions.setHandleRequest("webRequest.eventHandled",
      function() {
    var args = Array.prototype.slice.call(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });

  apiFunctions.setHandleRequest("webRequest.handlerBehaviorChanged",
      function() {
    var args = Array.prototype.slice.call(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });
});

})();
