// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module provides the test page for WebUIMojoTest. Once the page is
// loaded it sends "syn" message to the native code. Once page receives "ack"
// from the native code, the page then sends "fin". Test succeeds only when
// "fin" is received by the native page. Refer to
// ios/web/webui/web_ui_mojo_inttest.mm for testing code.

define('main', [
  'mojo/public/js/bindings',
  'mojo/public/js/core',
  'mojo/public/js/connection',
  'ios/web/test/mojo_test.mojom',
  'content/public/renderer/frame_service_registry',
], function(bindings, core, connection, browser, serviceRegistry) {

  var page;

  function TestPageImpl(browser) {
    this.browser_ = browser;
  };

  TestPageImpl.prototype = Object.create(browser.TestPage.stubClass.prototype);

  /**
   * Sends message as a string to the native code.
   *
   * @param {string} message Message to send.
   */
  TestPageImpl.prototype.sendMessage = function(message) {
    var pipe = core.createMessagePipe();
    var stub = connection.bindHandleToStub(pipe.handle0, browser.TestPage);
    bindings.StubBindings(stub).delegate = page;
    page.stub_ = stub;
    this.browser_.handleJsMessage(message, pipe.handle1);
  };

  /**
   * Called by native code with "ack" message.
   *
   * @param {!NativeMessageResultMojo} result Object received from the native
       code.
   */
  TestPageImpl.prototype.handleNativeMessage = function(result) {
    if (result.message == 'ack') {
      // Native code has replied with "ack", send "fin" to complete the test.
      this.sendMessage('fin');
    }
  };

  return function() {
    var browserProxy = connection.bindHandleToProxy(
        serviceRegistry.connectToService(browser.TestUIHandlerMojo.name),
        browser.TestUIHandlerMojo);

    page = new TestPageImpl(browserProxy);

    // Send "syn" so native code should reply with "ack".
    page.sendMessage('syn');
  };
});
