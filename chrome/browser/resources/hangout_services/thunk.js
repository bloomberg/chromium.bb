// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(
    function(message, sender, sendResponse) {
  function doSendResponse(value, error) {
    var errorMessage = error || chrome.extension.lastError;
    sendResponse({'value': value, 'error': errorMessage});
  }

  function getHost(url) {
    if (!url)
      return '';
    // Use the DOM to parse the URL. Since we don't add the anchor to
    // the page, this is the only reference to it and it will be
    // deleted once it's gone out of scope.
    var a = document.createElement('a');
    a.href = url;
    var origin = a.protocol + '//' + a.hostname;
    if (a.port != '')
      origin = origin + ':' + a.port;
    origin = origin + '/';
    return origin;
  }

  try {
    var method = message['method'];
    var origin = getHost(sender.url);
    if (method == 'chooseDesktopMedia') {
      var cancelId;
      function sendResponseWithCancelId(streamId) {
        var value = {'cancelId': cancelId,
                     'streamId': streamId};
        doSendResponse(value);
      }
      cancelId = chrome.desktopCapture.chooseDesktopMedia(
          ['screen', 'window'], sender.tab, sendResponseWithCancelId);
      return true;
    } else if (method == 'cancelChooseDesktopMedia') {
      var cancelId = message['cancelId'];
      chrome.desktopCapture.cancelChooseDesktopMedia(cancelId);
      doSendResponse();
      return false;
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
    } else if (method == 'logging.noUploadOnRenderClose') {
      chrome.webrtcLoggingPrivate.setUploadOnRenderClose(
          sender.tab.id, origin, false);
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
    } else if (method == 'isExtensionEnabled') {
      // This method is necessary because there may be more than one
      // version of this extension, under different extension IDs. By
      // first calling this method on the extension ID, the client can
      // check if it's loaded; if it's not, the extension system will
      // call the callback with no arguments and set
      // chrome.runtime.lastError.
      doSendResponse();
      return false;
    }
    throw new Error('Unknown method: ' + method);
  } catch (e) {
    doSendResponse(null, e.name + ': ' + e.message);
  }
});

// If Hangouts connects with a port named 'onSinksChangedListener', we
// will register a listener and send it a message {'eventName':
// 'onSinksChanged'} whenever the event fires.
chrome.runtime.onConnectExternal.addListener(
    function(port) {
  if (port.name == 'onSinksChangedListener') {
    function clientListener() {
      port.postMessage({'eventName': 'onSinksChanged'});
    }
    chrome.webrtcAudioPrivate.onSinksChanged.addListener(clientListener);

    port.onDisconnect(function() {
        chrome.webrtcAudioPrivate.onSinksChanged.removeListener(
            clientListener);
      });
  }
});
