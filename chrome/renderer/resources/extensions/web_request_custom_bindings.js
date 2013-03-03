// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the webRequest API.

var webRequestNatives = requireNative('web_request');
var GetUniqueSubEventName = webRequestNatives.GetUniqueSubEventName;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var validate = require('schemaUtils').validate;

// WebRequestEvent object. This is used for special webRequest events with
// extra parameters. Each invocation of addListener creates a new named
// sub-event. That sub-event is associated with the extra parameters in the
// browser process, so that only it is dispatched when the main event occurs
// matching the extra parameters.
//
// Example:
//   chrome.webRequest.onBeforeRequest.addListener(
//       callback, {urls: 'http://*.google.com/*'});
//   ^ callback will only be called for onBeforeRequests matching the filter.
function WebRequestEvent(eventName, opt_argSchemas, opt_extraArgSchemas,
                         opt_eventOptions) {
  if (typeof eventName != 'string')
    throw new Error('chrome.WebRequestEvent requires an event name.');

  this.eventName_ = eventName;
  this.argSchemas_ = opt_argSchemas;
  this.extraArgSchemas_ = opt_extraArgSchemas;
  this.subEvents_ = [];
  this.eventOptions_ = chromeHidden.parseEventOptions(opt_eventOptions);
  if (this.eventOptions_.supportsRules) {
    this.eventForRules_ =
        new chrome.Event(eventName, opt_argSchemas, opt_eventOptions);
  }
};

// Test if the given callback is registered for this event.
WebRequestEvent.prototype.hasListener = function(cb) {
  if (!this.eventOptions_.supportsListeners)
    throw new Error('This event does not support listeners.');
  return this.findListener_(cb) > -1;
};

// Test if any callbacks are registered fur thus event.
WebRequestEvent.prototype.hasListeners = function() {
  if (!this.eventOptions_.supportsListeners)
    throw new Error('This event does not support listeners.');
  return this.subEvents_.length > 0;
};

// Registers a callback to be called when this event is dispatched. If
// opt_filter is specified, then the callback is only called for events that
// match the given filters. If opt_extraInfo is specified, the given optional
// info is sent to the callback.
WebRequestEvent.prototype.addListener =
    function(cb, opt_filter, opt_extraInfo) {
  if (!this.eventOptions_.supportsListeners)
    throw new Error('This event does not support listeners.');
  // NOTE(benjhayden) New APIs should not use this subEventName trick! It does
  // not play well with event pages. See downloads.onDeterminingFilename and
  // ExtensionDownloadsEventRouter for an alternative approach.
  var subEventName = GetUniqueSubEventName(this.eventName_);
  // Note: this could fail to validate, in which case we would not add the
  // subEvent listener.
  validate(Array.prototype.slice.call(arguments, 1), this.extraArgSchemas_);
  chromeHidden.internalAPIs.webRequestInternal.addEventListener(
      cb, opt_filter, opt_extraInfo, this.eventName_, subEventName);

  var subEvent = new chrome.Event(subEventName, this.argSchemas_);
  var subEventCallback = cb;
  if (opt_extraInfo && opt_extraInfo.indexOf('blocking') >= 0) {
    var eventName = this.eventName_;
    subEventCallback = function() {
      var requestId = arguments[0].requestId;
      try {
        var result = cb.apply(null, arguments);
        chromeHidden.internalAPIs.webRequestInternal.eventHandled(
            eventName, subEventName, requestId, result);
      } catch (e) {
        chromeHidden.internalAPIs.webRequestInternal.eventHandled(
            eventName, subEventName, requestId);
        throw e;
      }
    };
  } else if (opt_extraInfo && opt_extraInfo.indexOf('asyncBlocking') >= 0) {
    var eventName = this.eventName_;
    subEventCallback = function() {
      var details = arguments[0];
      var requestId = details.requestId;
      var handledCallback = function(response) {
        chromeHidden.internalAPIs.webRequestInternal.eventHandled(
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
  if (!this.eventOptions_.supportsListeners)
    throw new Error('This event does not support listeners.');
  var idx;
  while ((idx = this.findListener_(cb)) >= 0) {
    var e = this.subEvents_[idx];
    e.subEvent.removeListener(e.subEventCallback);
    if (e.subEvent.hasListeners()) {
      console.error(
          'Internal error: webRequest subEvent has orphaned listeners.');
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
      console.error('Internal error: webRequest subEvent has no callback.');
    }
  }

  return -1;
};

WebRequestEvent.prototype.addRules = function(rules, opt_cb) {
  if (!this.eventOptions_.supportsRules)
    throw new Error('This event does not support rules.');
  this.eventForRules_.addRules(rules, opt_cb);
}

WebRequestEvent.prototype.removeRules = function(ruleIdentifiers, opt_cb) {
  if (!this.eventOptions_.supportsRules)
    throw new Error('This event does not support rules.');
  this.eventForRules_.removeRules(ruleIdentifiers, opt_cb);
}

WebRequestEvent.prototype.getRules = function(ruleIdentifiers, cb) {
  if (!this.eventOptions_.supportsRules)
    throw new Error('This event does not support rules.');
  this.eventForRules_.getRules(ruleIdentifiers, cb);
}

chromeHidden.registerCustomEvent('webRequest', WebRequestEvent);

chromeHidden.registerCustomHook('webRequest', function(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('handlerBehaviorChanged', function() {
    var args = Array.prototype.slice.call(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });
});
