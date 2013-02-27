// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var port = null;

function gotNativeMessage(message) {
  document.getElementById('response').innerHTML = "<p>Message Number: " +
                                                  message.id +
                                                  "</p><p>Message Text: " +
                                                  JSON.stringify(message.echo) +
                                                  "</p>";
}

function sendNativeMessage() {
  if (!port) {
    app = navigator.platform.match(/win/i) ? 'echo.bat' : 'echo.py';
    port = chrome.extension.connectNative(app);
    port.onMessage.addListener(gotNativeMessage);
    document.getElementById('input-text').style.display = 'block';
    document.getElementById('send-native-message').innerHTML = 'Send Message';
  }

  port.postMessage({"message": document.getElementById('input-text').value});
}

document.addEventListener('DOMContentLoaded', function () {
  document.getElementById('input-text').style.display = 'none';
  document.getElementById('send-native-message').addEventListener(
      'click', sendNativeMessage);
});
