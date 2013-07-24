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

var WebRequestEvent = require('webRequestInternal').WebRequestEvent;
var webRequestSchema =
    requireNative('schema_registry').GetSchema('webRequest');
var WebView = require('webView').WebView;

/**
 * @private
 */
WebView.prototype.maybeSetupExperimentalAPI_ = function() {
  this.setupWebRequestEvents_();
  this.setupDialogEvent_();
};

/**
 * @private
 */
WebView.prototype.maybeGetExperimentalPermissionTypes_ = function() {
  var PERMISSION_TYPES = ['download'];
  return PERMISSION_TYPES;
};

/**
 * @private
 */
WebView.prototype.setupWebRequestEvents_ = function() {
  var self = this;
  // Populate the WebRequest events from the API definition.
  for (var i = 0; i < webRequestSchema.events.length; ++i) {
    Object.defineProperty(self.webviewNode_,
                          webRequestSchema.events[i].name, {
      get: function(webRequestEvent) {
        return function() {
          if (!self[webRequestEvent.name + '_']) {
            self[webRequestEvent.name + '_'] =
                new WebRequestEvent(
                    'webview.' + webRequestEvent.name,
                    webRequestEvent.parameters,
                    webRequestEvent.extraParameters, null,
                    self.browserPluginNode_.getInstanceId());
          }
          return self[webRequestEvent.name + '_'];
        }
      }(webRequestSchema.events[i]),
      // No setter.
      enumerable: true
    });
  }
};

/**
 * @private
 */
WebView.prototype.setupDialogEvent_ = function() {
  var ERROR_MSG_DIALOG_ACTION_ALREADY_TAKEN = '<webview>: ' +
      'An action has already been taken for this "dialog" event.';

  var WARNING_MSG_DIALOG_BLOCKED = '<webview>: A dialog was blocked.';

  var DIALOG_EVENT_ATTRIBUTES = [
    'defaultPromptText',
    'messageText',
    'messageType',
    'url'
  ];

  var self = this;
  var node = this.webviewNode_;
  var browserPluginNode = this.browserPluginNode_;

  var onTrackedObjectGone = function(requestId, e) {
    var detail = e.detail ? JSON.parse(e.detail) : {};
    if (detail.id != requestId)
      return;
    browserPluginNode['-internal-setPermission'](requestId, false, '');
  }

  browserPluginNode.addEventListener('-internal-dialog', function(e) {
    var evt = new Event('dialog', { bubbles: true, cancelable: true });
    var detail = e.detail ? JSON.parse(e.detail) : {};

    $Array.forEach(DIALOG_EVENT_ATTRIBUTES, function(attribName) {
      evt[attribName] = detail[attribName];
    });
    var requestId = detail.requestId;
    var actionTaken = false;

    var validateCall = function() {
      if (actionTaken)
        throw new Error(ERROR_MSG_DIALOG_ACTION_ALREADY_TAKEN);
      actionTaken = true;
    };

    var dialog = {
      ok: function(user_input) {
        validateCall();
        browserPluginNode['-internal-setPermission'](
            requestId, true, user_input);
      },
      cancel: function() {
        validateCall();
        browserPluginNode['-internal-setPermission'](requestId, false, '');
      }
    };
    evt.dialog = dialog;
    // Make browser plugin track lifetime of |window|.
    var onTrackedObjectGoneWithRequestId =
        $Function.bind(onTrackedObjectGone, self, requestId);
    browserPluginNode.addEventListener('-internal-trackedobjectgone',
        onTrackedObjectGoneWithRequestId);
    browserPluginNode['-internal-trackObjectLifetime'](dialog, requestId);

    var defaultPrevented = !node.dispatchEvent(evt);
    if (!actionTaken && !defaultPrevented) {
      actionTaken = true;
      // The default action is equivalent to canceling the dialog.
      browserPluginNode['-internal-setPermission'](requestId, false, '');
      console.warn(WARNING_MSG_DIALOG_BLOCKED);
    }
  });
};
