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
