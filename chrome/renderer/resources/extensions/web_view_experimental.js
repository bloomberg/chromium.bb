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

var CreateEvent = require('webView').CreateEvent;
var MessagingNatives = requireNative('messaging_natives');
var WebViewInternal = require('webView').WebViewInternal;
var WebView = require('webView').WebView;

var WEB_VIEW_EXPERIMENTAL_EVENTS = {
  'dialog': {
    cancelable: true,
    customHandler: function(webViewInternal, event, webViewEvent) {
      webViewInternal.handleDialogEvent_(event, webViewEvent);
    },
    evt: CreateEvent('webview.onDialog'),
    fields: ['defaultPromptText', 'messageText', 'messageType', 'url']
  }
};

/**
 * @private
 */
WebViewInternal.prototype.maybeAttachWebRequestEventToObject_ =
    function(obj, eventName, webRequestEvent) {
  Object.defineProperty(
      obj,
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
WebViewInternal.prototype.handleDialogEvent_ =
    function(event, webViewEvent) {
  var showWarningMessage = function(dialogType) {
    var VOWELS = ['a', 'e', 'i', 'o', 'u'];
    var WARNING_MSG_DIALOG_BLOCKED = '<webview>: %1 %2 dialog was blocked.';
    var article = (VOWELS.indexOf(dialogType.charAt(0)) >= 0) ? 'An' : 'A';
    var output = WARNING_MSG_DIALOG_BLOCKED.replace('%1', article);
    output = output.replace('%2', dialogType);
    window.console.warn(output);
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
      WebView.setPermission(self.instanceId_, requestId, 'allow', user_input);
    },
    cancel: function() {
      validateCall();
      WebView.setPermission(self.instanceId_, requestId, 'deny');
    }
  };
  webViewEvent.dialog = dialog;

  var defaultPrevented = !webviewNode.dispatchEvent(webViewEvent);
  if (actionTaken) {
    return;
  }

  if (defaultPrevented) {
    // Tell the JavaScript garbage collector to track lifetime of |dialog| and
    // call back when the dialog object has been collected.
    MessagingNatives.BindToGC(dialog, function() {
      // Avoid showing a warning message if the decision has already been made.
      if (actionTaken) {
        return;
      }
      WebView.setPermission(
          self.instanceId_, requestId, 'default', '', function(allowed) {
        if (allowed) {
          return;
        }
        showWarningMessage(event.messageType);
      });
    });
  } else {
    actionTaken = true;
    // The default action is equivalent to canceling the dialog.
    WebView.setPermission(
        self.instanceId_, requestId, 'default', '', function(allowed) {
      if (allowed) {
        return;
      }
      showWarningMessage(event.messageType);
    });
  }
};

/** @private */
WebViewInternal.prototype.maybeGetExperimentalEvents_ = function() {
  return WEB_VIEW_EXPERIMENTAL_EVENTS;
};

WebViewInternal.prototype.maybeGetExperimentalPermissions_ = function() {
  return [];
};

/** @private */
WebViewInternal.prototype.clearData_ = function(var_args) {
  if (!this.instanceId_) {
    return;
  }
  var args = $Array.concat([this.instanceId_], $Array.slice(arguments));
  $Function.apply(WebView.clearData, null, args);
};

/** @private */
WebViewInternal.prototype.getUserAgent_ = function() {
  return this.userAgentOverride_ || navigator.userAgent;
};

/** @private */
WebViewInternal.prototype.isUserAgentOverridden_ = function() {
  return !!this.userAgentOverride_ &&
      this.userAgentOverride_ != navigator.userAgent;
};

/** @private */
WebViewInternal.prototype.setUserAgentOverride_ = function(userAgentOverride) {
  this.userAgentOverride_ = userAgentOverride;
  if (!this.instanceId_) {
    // If we are not attached yet, then we will pick up the user agent on
    // attachment.
    return;
  }
  WebView.overrideUserAgent(this.instanceId_, userAgentOverride);
};

/** @private */
WebViewInternal.prototype.captureVisibleRegion_ = function(spec, callback) {
  WebView.captureVisibleRegion(this.instanceId_, spec, callback);
};

WebViewInternal.maybeRegisterExperimentalAPIs = function(proto, secret) {
  proto.clearData = function(var_args) {
    var internal = this.internal_(secret);
    $Function.apply(internal.clearData_, internal, arguments);
  };

  proto.getUserAgent = function() {
    return this.internal_(secret).getUserAgent_();
  };

  proto.isUserAgentOverridden = function() {
    return this.internal_(secret).isUserAgentOverridden_();
  };

  proto.setUserAgentOverride = function(userAgentOverride) {
    this.internal_(secret).setUserAgentOverride_(userAgentOverride);
  };

  proto.captureVisibleRegion = function(spec, callback) {
    this.internal_(secret).captureVisibleRegion_(spec, callback);
  };
};
