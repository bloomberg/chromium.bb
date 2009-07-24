var win = window;
if (typeof(contentWindow) != 'undefined') {
  win = contentWindow;
}

win.onload = function() {
  // Do this in an onload handler because I'm not sure if chrome.extension
  // is available before then.
  chrome.extension.onConnect.addListener(function(port) {
    console.log('connected');
    port.onMessage.addListener(function(msg) {
      console.log('got ' + msg);
      if (msg.testPostMessage) {
        port.postMessage({success: true});
      } else if (msg.testDisconnect) {
        port.disconnect();
      } else if (msg.testDisconnectOnClose) {
        win.location = "about:blank";
      }
      // Ignore other messages since they are from us.
    });
  });
}

// Tests that postMessage to the extension and its response works.
win.testPostMessageFromTab = function() {
  var port = chrome.extension.connect();
  port.postMessage({testPostMessageFromTab: true});
  port.onMessage.addListener(function(msg) {
    win.domAutomationController.send(msg.success);
    console.log('sent ' + msg.success);
    port.disconnect();
  });
  console.log('posted message');
}

// Workaround two bugs: shutdown crash if we hook 'unload', and content script
// GC if we don't register any event handlers.
// http://code.google.com/p/chromium/issues/detail?id=17410
// http://code.google.com/p/chromium/issues/detail?id=17582
function foo() {}
win.addEventListener('error', foo);
