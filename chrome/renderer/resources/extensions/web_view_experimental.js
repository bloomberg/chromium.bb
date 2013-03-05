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

var WebView = require('webview').WebView;

/** @type {string} */
var REQUEST_TYPE_MEDIA = 'media';

/** @type {string} */
var ERROR_MSG_PERMISSION_ALREADY_DECIDED = '<webview>: ' +
    'Permission has already been decided for this "permissionrequest" event.';

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
    ['permission', 'url'].forEach(function(attribName) {
      evt[attribName] = detail[attribName];
    });
    var requestId = detail.requestId;

    if (detail.permission == REQUEST_TYPE_MEDIA &&
        detail.requestId !== undefined) {
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
          request, REQUEST_TYPE_MEDIA, requestId);

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
