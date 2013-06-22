// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webRequestInternal API.

var binding = require('binding').Binding.create('webRequestInternal');
var eventBindings = require('event_bindings');
var sendRequest = require('sendRequest').sendRequest;
var validate = require('schemaUtils').validate;
var webRequestNatives = requireNative('web_request');

var webRequestInternal;

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
                         opt_eventOptions, opt_webViewInstanceId) {
  if (typeof eventName != 'string')
    throw new Error('chrome.WebRequestEvent requires an event name.');

  this.eventName_ = eventName;
  this.argSchemas_ = opt_argSchemas;
  this.extraArgSchemas_ = opt_extraArgSchemas;
  this.webViewInstanceId_ = opt_webViewInstanceId ? opt_webViewInstanceId : 0;
  this.subEvents_ = [];
  this.eventOptions_ = eventBindings.parseEventOptions(opt_eventOptions);
  if (this.eventOptions_.supportsRules) {
    this.eventForRules_ =
        new eventBindings.Event(eventName, opt_argSchemas, opt_eventOptions);
  }
}

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
  var subEventName = webRequestNatives.GetUniqueSubEventName(this.eventName_);
  // Note: this could fail to validate, in which case we would not add the
  // subEvent listener.
  validate($Array.slice(arguments, 1), this.extraArgSchemas_);
  webRequestInternal.addEventListener(
      cb, opt_filter, opt_extraInfo, this.eventName_, subEventName,
      this.webViewInstanceId_);

  var subEvent = new eventBindings.Event(subEventName, this.argSchemas_);
  var subEventCallback = cb;
  if (opt_extraInfo && opt_extraInfo.indexOf('blocking') >= 0) {
    var eventName = this.eventName_;
    subEventCallback = function() {
      var requestId = arguments[0].requestId;
      try {
        var result = $Function.apply(cb, null, arguments);
        webRequestInternal.eventHandled(
            eventName, subEventName, requestId, result);
      } catch (e) {
        webRequestInternal.eventHandled(
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
        webRequestInternal.eventHandled(
            eventName, subEventName, requestId, response);
      };
      $Function.apply(cb, null, [details, handledCallback]);
    };
  }
  $Array.push(this.subEvents_,
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
    $Array.splice(this.subEvents_, idx, 1);
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
};

WebRequestEvent.prototype.removeRules = function(ruleIdentifiers, opt_cb) {
  if (!this.eventOptions_.supportsRules)
    throw new Error('This event does not support rules.');
  this.eventForRules_.removeRules(ruleIdentifiers, opt_cb);
};

WebRequestEvent.prototype.getRules = function(ruleIdentifiers, cb) {
  if (!this.eventOptions_.supportsRules)
    throw new Error('This event does not support rules.');
  this.eventForRules_.getRules(ruleIdentifiers, cb);
};

binding.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('addEventListener', function() {
    var args = $Array.slice(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });

  apiFunctions.setHandleRequest('eventHandled', function() {
    var args = $Array.slice(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {forIOThread: true});
  });
});

webRequestInternal = binding.generate();
exports.binding = webRequestInternal;
exports.WebRequestEvent = WebRequestEvent;
