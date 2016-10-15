// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Module "content/public/renderer/frame_interfaces"
//
// This module provides the JavaScript bindings for
// services/service_manager/public/cpp/connection.h.
// Refer to that file for more detailed documentation for equivalent methods.

define("content/public/renderer/frame_interfaces", [
  "ios/mojo/public/js/sync_message_channel",
  "ios/mojo/public/js/handle_util",
], function(syncMessageChannel, handleUtil) {

  /**
   * Binds to specified Mojo interface.
   * @param {string} getInterface interface name to connect.
   * @return {!MojoResult} Result code.
   */
  function getInterface(interfaceName) {
    var nativeHandle = syncMessageChannel.sendMessage({
      name: "interface_provider.getInterface",
      args: {
        interfaceName: interfaceName
      }
    });
    return handleUtil.getJavaScriptHandle(nativeHandle);
  }

  var exports = {};
  exports.getInterface = getInterface;
  return exports;
});
