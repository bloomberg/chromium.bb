// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    function(message, sender, sendResponse) {
  function doSendResponse(value, error) {
    var errorMessage = error || chrome.extension.lastError;
    sendResponse({'value': value, 'error': errorMessage});
  }
  try {
    var method = message['method'];
    var origin = message['origin'];
    if (method == 'chooseDesktopMedia') {
      chrome.desktopCapture.chooseDesktopMedia(
          ['screen', 'window'], sender.tab, doSendResponse);
      return true;
    } else if (method == 'cpu.getInfo') {
      chrome.system.cpu.getInfo(doSendResponse);
      return true;
    } else if (method == 'logging.setMetadata') {
      var metaData = message['metaData'];
      chrome.webrtcLoggingPrivate.setMetaData(
          sender.tab.id, origin, metaData, doSendResponse);
      return true;
    } else if (method == 'logging.start') {
      chrome.webrtcLoggingPrivate.start(sender.tab.id, origin, doSendResponse);
      return true;
    } else if (method == 'logging.uploadOnRenderClose') {
      chrome.webrtcLoggingPrivate.setUploadOnRenderClose(
          sender.tab.id, origin, true);
      doSendResponse();
      return false;
    } else if (method == 'logging.stop') {
      chrome.webrtcLoggingPrivate.stop(sender.tab.id, origin, doSendResponse);
      return true;
    } else if (method == 'logging.upload') {
      chrome.webrtcLoggingPrivate.upload(sender.tab.id, origin, doSendResponse);
      return true;
    } else if (method == 'logging.discard') {
      chrome.webrtcLoggingPrivate.discard(
          sender.tab.id, origin, doSendResponse);
      return true;
    } else if (method == 'getSinks') {
      chrome.webrtcAudioPrivate.getSinks(doSendResponse);
      return true;
    } else if (method == 'getActiveSink') {
      chrome.webrtcAudioPrivate.getActiveSink(sender.tab.id, doSendResponse);
      return true;
    } else if (method == 'setActiveSink') {
      var sinkId = message['sinkId'];
      chrome.webrtcAudioPrivate.setActiveSink(
          sender.tab.id, sinkId, doSendResponse);
      return true;
    } else if (method == 'getAssociatedSink') {
      var sourceId = message['sourceId'];
      chrome.webrtcAudioPrivate.getAssociatedSink(
          origin, sourceId, doSendResponse);
      return true;
    }
    throw new Error('Unknown method: ' + method);
  } catch (e) {
    doSendResponse(null, e.name + ': ' + e.message);
  }
});
