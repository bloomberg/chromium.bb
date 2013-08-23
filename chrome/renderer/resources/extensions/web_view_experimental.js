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
var messagingNatives = requireNative('messaging_natives');
var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var webRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var WebViewInternal = require('webView').WebViewInternal;
var webView = require('webView').webView;

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
WebViewInternal.prototype.maybeAttachWebRequestEventToWebview_ =
    function(eventName, webRequestEvent) {
  Object.defineProperty(
      this.webviewNode_,
      eventName,
      {
        get: webRequestEvent,
        enumerable: true
      }
  );
};

/**
 * @private
 */
WebViewInternal.prototype.maybeSetupExtDialogEvent_ =
    function(event, webviewEvent) {
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
      webView.setPermission(self.instanceId_, requestId, true, user_input);
    },
    cancel: function() {
      validateCall();
      webView.setPermission(self.instanceId_, requestId, false, '');
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
    messagingNatives.BindToGC(dialog, function() {
      // Avoid showing a warning message if the decision has already been made.
      if (actionTaken) {
        return;
      }
      webView.setPermission(self.instanceId_, requestId, false, '');
      showWarningMessage(event.messageType);
    });
  } else {
    actionTaken = true;
    // The default action is equivalent to canceling the dialog.
    webView.setPermission(self.instanceId_, requestId, false, '');
    showWarningMessage(event.messageType);
  }
};

/**
 * @private
 */
WebViewInternal.prototype.maybeGetWebviewExperimentalExtEvents_ = function() {
  return WEB_VIEW_EXPERIMENTAL_EXT_EVENTS;
};

WebViewInternal.prototype.clearData_ = function(var_args) {
  if (!this.instanceId_) {
    return;
  }
  var args = $Array.concat([this.instanceId_], $Array.slice(arguments));
  $Function.apply(webView.clearData, null, args);
};

WebViewInternal.maybeRegisterExperimentalAPIs = function(proto, secret) {
  proto.clearData = function(var_args) {
    var internal = this.internal_(secret);
    $Function.apply(internal.clearData_, internal, arguments);
  };
};
