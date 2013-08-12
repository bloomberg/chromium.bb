// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Shim extension to provide permission request API (and possibly other future
// experimental APIs) for <webview> tag.
// See web_view.js for details.
//
// We want to control the permission API feature in <webview> separately from
// the <webview> feature itself. <webview> is available in stable channel, but
// permission API would only be available for channels CHANNEL_DEV and
// CHANNEL_CANARY.

var createEvent = require('webView').CreateEvent;
var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var webRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var WebView = require('webView').WebView;

var WEB_VIEW_EXPERIMENTAL_EXT_EVENTS = {
  'dialog': {
    cancelable: true,
    customHandler: function(webview, event, webviewEvent) {
      webview.maybeSetupExtDialogEvent_(event, webviewEvent);
    },
    evt: createEvent('webview.onDialog'),
    fields: ['defaultPromptText', 'messageText', 'messageType', 'url']
  }
};

/**
 * @private
 */
WebView.prototype.maybeSetupExperimentalAPI_ = function() {
  this.setupWebRequestEvents_();
};

/**
 * @private
 */
WebView.prototype.setupWebRequestEvents_ = function() {
  var self = this;
  var request = {};
  var createWebRequestEvent = function(webRequestEvent) {
    return function() {
      if (!self[webRequestEvent.name + '_']) {
        self[webRequestEvent.name + '_'] =
            new WebRequestEvent(
                'webview.' + webRequestEvent.name,
                webRequestEvent.parameters,
                webRequestEvent.extraParameters, null,
                self.viewInstanceId_);
      }
      return self[webRequestEvent.name + '_'];
    };
  };

  // Populate the WebRequest events from the API definition.
  for (var i = 0; i < webRequestSchema.events.length; ++i) {
    var webRequestEvent = createWebRequestEvent(webRequestSchema.events[i]);
    Object.defineProperty(
        request,
        webRequestSchema.events[i].name,
        {
          get: webRequestEvent,
          enumerable: true
        }
    );
    Object.defineProperty(
        this.webviewNode_,
        webRequestSchema.events[i].name,
        {
          get: webRequestEvent,
          enumerable: true
        }
    );
  }
  Object.defineProperty(
      this.webviewNode_,
      'request',
      {
        value: request,
        enumerable: true,
        writable: false
      }
  );
};

/**
 * @private
 */
WebView.prototype.maybeSetupExtDialogEvent_ = function(event, webviewEvent) {
  var showWarningMessage = function(dialogType) {
    var VOWELS = ['a', 'e', 'i', 'o', 'u'];
    var WARNING_MSG_DIALOG_BLOCKED = '<webview>: %1 %2 dialog was blocked.';
    var article = (VOWELS.indexOf(dialogType.charAt(0)) >= 0) ? 'An' : 'A';
    var output = WARNING_MSG_DIALOG_BLOCKED.replace('%1', article);
    output = output.replace('%2', dialogType);
    console.warn(output);
  };

  var self = this;
  var browserPluginNode = this.browserPluginNode_;
  var webviewNode = this.webviewNode_;

  var requestId = event.requestId;
  var actionTaken = false;

  var onTrackedObjectGone = function(requestId, dialogType, e) {
    var detail = e.detail ? JSON.parse(e.detail) : {};
    if (detail.id != requestId) {
      return;
    }

    // Avoid showing a warning message if the decision has already been made.
    if (actionTaken) {
      return;
    }

    chrome.webview.setPermission(self.instanceId_, requestId, false, '');
    showWarningMessage(dialogType);
  }

  var validateCall = function() {
    var ERROR_MSG_DIALOG_ACTION_ALREADY_TAKEN = '<webview>: ' +
        'An action has already been taken for this "dialog" event.';

    if (actionTaken) {
      throw new Error(ERROR_MSG_DIALOG_ACTION_ALREADY_TAKEN);
    }
    actionTaken = true;
  };

  var dialog = {
    ok: function(user_input) {
      validateCall();
      user_input = user_input || '';
      chrome.webview.setPermission(
          self.instanceId_, requestId, true, user_input);
    },
    cancel: function() {
      validateCall();
      chrome.webview.setPermission(self.instanceId_, requestId, false, '');
    }
  };
  webviewEvent.dialog = dialog;

  var defaultPrevented = !webviewNode.dispatchEvent(webviewEvent);
  if (actionTaken) {
    return;
  }

  if (defaultPrevented) {
    // Tell the JavaScript garbage collector to track lifetime of |dialog| and
    // call back when the dialog object has been collected.
    var onTrackedObjectGoneWithRequestId =
        $Function.bind(
            onTrackedObjectGone, self, requestId, event.messageType);
    browserPluginNode.addEventListener('-internal-trackedobjectgone',
        onTrackedObjectGoneWithRequestId);
    browserPluginNode['-internal-trackObjectLifetime'](dialog, requestId);
  } else {
    actionTaken = true;
    // The default action is equivalent to canceling the dialog.
    chrome.webview.setPermission(self.instanceId_, requestId, false, '');
    showWarningMessage(event.messageType);
  }
};

/**
 * @private
 */
WebView.prototype.maybeGetWebviewExperimentalExtEvents_ = function() {
  return WEB_VIEW_EXPERIMENTAL_EXT_EVENTS;
};
