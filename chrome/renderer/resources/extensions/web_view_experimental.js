// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements experimental API for <webview>.
// See web_view.js for details.
//
// <webview> Experimental API is only available on canary and dev channels of
// Chrome.

var CreateEvent = require('webView').CreateEvent;
var MessagingNatives = requireNative('messaging_natives');
var WebViewInternal = require('webView').WebViewInternal;
var WebView = require('webView').WebView;
var utils = require('utils');

// WEB_VIEW_EXPERIMENTAL_EVENTS is a map of experimental <webview> DOM event
//     names to their associated extension event descriptor objects.
// An event listener will be attached to the extension event |evt| specified in
//     the descriptor.
// |fields| specifies the public-facing fields in the DOM event that are
//     accessible to <webview> developers.
// |customHandler| allows a handler function to be called each time an extension
//     event is caught by its event listener. The DOM event should be dispatched
//     within this handler function. With no handler function, the DOM event
//     will be dispatched by default each time the extension event is caught.
// |cancelable| (default: false) specifies whether the event's default
//     behavior can be canceled. If the default action associated with the event
//     is prevented, then its dispatch function will return false in its event
//     handler. The event must have a custom handler for this to be meaningful.
var WEB_VIEW_EXPERIMENTAL_EVENTS = {
  'dialog': {
    cancelable: true,
    customHandler: function(webViewInternal, event, webViewEvent) {
      webViewInternal.handleDialogEvent(event, webViewEvent);
    },
    evt: CreateEvent('webview.onDialog'),
    fields: ['defaultPromptText', 'messageText', 'messageType', 'url']
  },
  'findupdate': {
    evt: CreateEvent('webview.onFindReply'),
    fields: [
      'searchText',
      'numberOfMatches',
      'activeMatchOrdinal',
      'selectionRect',
      'canceled',
      'finalUpdate'
    ]
  },
  'zoomchange': {
    evt: CreateEvent('webview.onZoomChange'),
    fields: ['oldZoomFactor', 'newZoomFactor']
  }
};

/**
 * An instance of this class is exposed as <webview>.contextMenus.
 * @constructor
 */
function WebViewContextMenusImpl(viewInstanceId) {
  this.viewInstanceId_ = viewInstanceId;
};

WebViewContextMenusImpl.prototype.create = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(WebView.contextMenusCreate, null, args);
};

WebViewContextMenusImpl.prototype.remove = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(WebView.contextMenusRemove, null, args);
};

WebViewContextMenusImpl.prototype.removeAll = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(WebView.contextMenusRemoveAll, null, args);
};

WebViewContextMenusImpl.prototype.update = function() {
  var args = $Array.concat([this.viewInstanceId_], $Array.slice(arguments));
  return $Function.apply(WebView.contextMenusUpdate, null, args);
};

var WebViewContextMenus = utils.expose(
    'WebViewContextMenus', WebViewContextMenusImpl,
    ['create', 'remove', 'removeAll', 'update']);

/**
 * @private
 */
WebViewInternal.prototype.maybeAttachWebRequestEventToObject =
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
WebViewInternal.prototype.setZoom = function(zoomFactor) {
  if (!this.instanceId) {
    return;
  }
  WebView.setZoom(this.instanceId, zoomFactor);
};

/**
 * @private
 */
WebViewInternal.prototype.handleDialogEvent =
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
  var browserPluginNode = this.browserPluginNode;
  var webviewNode = this.webviewNode;

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
      WebView.setPermission(self.instanceId, requestId, 'allow', user_input);
    },
    cancel: function() {
      validateCall();
      WebView.setPermission(self.instanceId, requestId, 'deny');
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
          self.instanceId, requestId, 'default', '', function(allowed) {
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
        self.instanceId, requestId, 'default', '', function(allowed) {
      if (allowed) {
        return;
      }
      showWarningMessage(event.messageType);
    });
  }
};

WebViewInternal.prototype.maybeGetExperimentalEvents = function() {
  return WEB_VIEW_EXPERIMENTAL_EVENTS;
};

/** @private */
WebViewInternal.prototype.maybeGetExperimentalPermissions = function() {
  return [];
};

/** @private */
WebViewInternal.prototype.maybeSetCurrentZoomFactor =
    function(zoomFactor) {
  this.currentZoomFactor = zoomFactor;
};

/** @private */
WebViewInternal.prototype.clearData = function() {
  if (!this.instanceId) {
    return;
  }
  var args = $Array.concat([this.instanceId], $Array.slice(arguments));
  $Function.apply(WebView.clearData, null, args);
};

/** @private */
WebViewInternal.prototype.setZoom = function(zoomFactor, callback) {
  if (!this.instanceId) {
    return;
  }
  WebView.setZoom(this.instanceId, zoomFactor, callback);
};

WebViewInternal.prototype.getZoom = function(callback) {
  if (!this.instanceId) {
    return;
  }
  WebView.getZoom(this.instanceId, callback);
};

/** @private */
WebViewInternal.prototype.captureVisibleRegion = function(spec, callback) {
  WebView.captureVisibleRegion(this.instanceId, spec, callback);
};

/** @private */
WebViewInternal.prototype.find = function(search_text, options, callback) {
  if (!this.instanceId) {
    return;
  }
  WebView.find(this.instanceId, search_text, options, callback);
};

/** @private */
WebViewInternal.prototype.stopFinding = function(action) {
  if (!this.instanceId) {
    return;
  }
  WebView.stopFinding(this.instanceId, action);
};

WebViewInternal.maybeRegisterExperimentalAPIs = function(proto) {
  proto.clearData = function() {
    var internal = privates(this).internal;
    $Function.apply(internal.clearData, internal, arguments);
  };

  proto.setZoom = function(zoomFactor, callback) {
    privates(this).internal.setZoom(zoomFactor, callback);
  };

  proto.getZoom = function(callback) {
    return privates(this).internal.getZoom(callback);
  };

  proto.captureVisibleRegion = function(spec, callback) {
    privates(this).internal.captureVisibleRegion(spec, callback);
  };

  proto.find = function(search_text, options, callback) {
    privates(this).internal.find(search_text, options, callback);
  };

  proto.stopFinding = function(action) {
    privates(this).internal.stopFinding(action);
  };
};

/** @private */
WebViewInternal.prototype.setupExperimentalContextMenus_ = function() {
  var self = this;
  var createContextMenus = function() {
    return function() {
      if (self.contextMenus_) {
        return self.contextMenus_;
      }

      self.contextMenus_ = new WebViewContextMenus(self.viewInstanceId);
      // TODO(lazyboy): define 'onClicked' property/event on
      // |self.contextMenus_|.
      return self.contextMenus_;
    };
  };

  // Expose <webview>.contextMenus object.
  Object.defineProperty(
      this.webviewNode,
      'contextMenus',
      {
        get: createContextMenus(),
        enumerable: true
      });
};
