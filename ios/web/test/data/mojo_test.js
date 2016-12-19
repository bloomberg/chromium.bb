// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides the test page for WebUIMojoTest. Once the page is
// loaded it sends "syn" message to the native code. Once page receives "ack"
// from the native code, the page then sends "fin". Test succeeds only when
// "fin" is received by the native page. Refer to
// ios/web/webui/web_ui_mojo_inttest.mm for testing code.

/** @return {!Promise} */
function getBrowserProxy() {
  return new Promise(function(resolve, reject) {
    define([
      'mojo/public/js/bindings',
      'ios/web/test/mojo_test.mojom',
      'content/public/renderer/frame_interfaces',
    ], function(bindings, mojom, frameInterfaces) {
      var pageImpl, browserProxy;

      /** @constructor */
      function TestPageImpl() {
        this.binding = new bindings.Binding(mojom.TestPage, this);
      }

      TestPageImpl.prototype = {
        /** @override */
        handleNativeMessage: function(result) {
          if (result.message == 'ack') {
            // Native code has replied with "ack", send "fin" to complete the
            // test.
            browserProxy.handleJsMessage('fin');
          }
        },
      };

      browserProxy = new mojom.TestUIHandlerMojoPtr(
          frameInterfaces.getInterface(mojom.TestUIHandlerMojo.name));
      pageImpl = new TestPageImpl();

      browserProxy.setClientPage(pageImpl.binding.createInterfacePtrAndBind());
      resolve(browserProxy);
    });
  });
}

/**
 * @return {!Promise} Fires when DOMContentLoaded event is received.
 */
function whenDomContentLoaded() {
  return new Promise(function(resolve, reject) {
    document.addEventListener('DOMContentLoaded', resolve);
  });
}

function main() {
  Promise.all([
    getBrowserProxy(), whenDomContentLoaded()
  ]).then(function([browserProxy]) {
    // Send "syn" so native code should reply with "ack".
    browserProxy.handleJsMessage('syn');
  });
}
main();
