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

var WebView = require('webView').WebView;

var forEach = require('utils').forEach;

/** @type {Array.<string>} */
var PERMISSION_TYPES = ['download', 'media', 'geolocation', 'pointerLock'];

/** @type {string} */
var REQUEST_TYPE_NEW_WINDOW = 'newwindow';

/** @type {string} */
var ERROR_MSG_PERMISSION_ALREADY_DECIDED = '<webview>: ' +
    'Permission has already been decided for this "permissionrequest" event.';

var EXPOSED_PERMISSION_EVENT_ATTRIBS = [
    'lastUnlockedBySelf',
    'permission',
    'requestMethod',
    'url',
    'userGesture'
];

/** @type {string} */
var ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN = '<webview>: ' +
    'An action has already been taken for this "newwindow" event.';

/** @type {string} */
var ERROR_MSG_WEBVIEW_EXPECTED = '<webview> element expected.';

/**
 * @param {!Object} detail The event details, originated from <object>.
 * @private
 */
WebView.prototype.maybeSetupPermissionEvent_ = function() {
  var node = this.node_;
  var objectNode = this.objectNode_;
  this.objectNode_.addEventListener('-internal-permissionrequest', function(e) {
    var evt = new Event('permissionrequest', {bubbles: true, cancelable: true});
    var detail = e.detail ? JSON.parse(e.detail) : {};
    forEach(EXPOSED_PERMISSION_EVENT_ATTRIBS, function(i, attribName) {
      if (detail[attribName] !== undefined)
        evt[attribName] = detail[attribName];
    });
    var requestId = detail.requestId;

    if (detail.requestId !== undefined &&
        PERMISSION_TYPES.indexOf(detail.permission) >= 0) {
      // TODO(lazyboy): Also fill in evt.details (see webview specs).
      // http://crbug.com/141197.
      var decisionMade = false;
      // Construct the event.request object.
      var request = {
        allow: function() {
          if (decisionMade) {
            throw new Error(ERROR_MSG_PERMISSION_ALREADY_DECIDED);
          } else {
            objectNode['-internal-setPermission'](requestId, true);
            decisionMade = true;
          }
        },
        deny: function() {
          if (decisionMade) {
            throw new Error(ERROR_MSG_PERMISSION_ALREADY_DECIDED);
          } else {
            objectNode['-internal-setPermission'](requestId, false);
            decisionMade = true;
          }
        }
      };
      evt.request = request;

      // Make browser plugin track lifetime of |request|.
      objectNode['-internal-persistObject'](
          request, detail.permission, requestId);

      var defaultPrevented = !node.dispatchEvent(evt);
      if (!decisionMade && !defaultPrevented) {
        decisionMade = true;
        objectNode['-internal-setPermission'](requestId, false);
      }
    }
  });
};

/**
 * @private
 */
WebView.prototype.maybeSetupExecuteScript_ = function() {
  var self = this;
  this.node_['executeScript'] = function(var_args) {
    var args = [self.objectNode_.getProcessId(),
                self.objectNode_.getRouteId()].concat(
                    Array.prototype.slice.call(arguments));
    chrome.webview.executeScript.apply(null, args);
  }
};

/**
 * @private
 */
WebView.prototype.maybeSetupNewWindowEvent_ = function() {
  var NEW_WINDOW_EVENT_ATTRIBUTES = [
    'initialHeight',
    'initialWidth',
    'targetUrl',
    'windowOpenDisposition',
    'name'
  ];

  var node = this.node_;
  var objectNode = this.objectNode_;
  objectNode.addEventListener('-internal-newwindow', function(e) {
    var evt = new Event('newwindow', { bubbles: true, cancelable: true });
    var detail = e.detail ? JSON.parse(e.detail) : {};

    NEW_WINDOW_EVENT_ATTRIBUTES.forEach(function(attribName) {
      evt[attribName] = detail[attribName];
    });
    var requestId = detail.requestId;
    var actionTaken = false;

    var validateCall = function () {
      if (actionTaken)
        throw new Error(ERROR_MSG_NEWWINDOW_ACTION_ALREADY_TAKEN);
      actionTaken = true;
    };

    var window = {
      attach: function(webview) {
        validateCall();
        if (!webview)
          throw new Error(ERROR_MSG_WEBVIEW_EXPECTED);
        // Attach happens asynchronously to give the tagWatcher an opportunity
        // to pick up the new webview before attach operates on it, if it hasn't
        // been attached to the DOM already.
        // Note: Any subsequent errors cannot be exceptions because they happen
        // asynchronously.
        setTimeout(function() {
          var attached =
              objectNode['-internal-attachWindowTo'](webview, detail.windowId);
          if (!attached) {
            console.error('Unable to attach the new window to the provided ' +
                'webview.');
          }
          // If the object being passed into attach is not a valid <webview>
          // then we will fail and it will be treated as if the new window
          // was rejected. The permission API plumbing is used here to clean
          // up the state created for the new window if attaching fails.
          objectNode['-internal-setPermission'](requestId, attached);
        }, 0);
      },
      discard: function() {
        validateCall();
        objectNode['-internal-setPermission'](requestId, false);
      }
    };
    evt.window = window;
    // Make browser plugin track lifetime of |window|.
    objectNode['-internal-persistObject'](
        window, detail.permission, requestId);

    var defaultPrevented = !node.dispatchEvent(evt);
    if (!actionTaken && !defaultPrevented) {
      actionTaken = true;
      // The default action is to discard the window.
      objectNode['-internal-setPermission'](requestId, false);
    }
  });
};
