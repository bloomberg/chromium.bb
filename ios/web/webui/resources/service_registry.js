// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "content/public/renderer/frame_service_registry"
//
// This module provides the JavaScript bindings for
// services/shell/public/cpp/connection.h.
// Refer to that file for more detailed documentation for equivalent methods.

define("content/public/renderer/frame_service_registry", [
  "ios/mojo/public/js/sync_message_channel",
  "ios/mojo/public/js/handle_util",
], function(syncMessageChannel, handleUtil) {

  /**
   * Connects to specified Mojo interface.
   * @param {string} serviceName service name to connect.
   * @return {!MojoResult} Result code.
   */
  function connectToService(serviceName) {
    var nativeHandle = syncMessageChannel.sendMessage({
      name: "service_provider.connectToService",
      args: {
        serviceName: serviceName
      }
    });
    return handleUtil.getJavaScriptHandle(nativeHandle);
  }

  var exports = {};
  exports.connectToService = connectToService;
  return exports;
});
