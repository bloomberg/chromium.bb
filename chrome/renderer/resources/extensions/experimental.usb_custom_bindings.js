// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(gdk): This all looks very similar to the socket custom bindings, and for
// a good reason, because they essentially do the same work. Refactor onEvent
// bindings out of a per-extension mechanism into a generalized mechanism.

  var experimentalUsbNatives = requireNative('experimental_usb');
  var GetNextUsbEventId = experimentalUsbNatives.GetNextUsbEventId;

  var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
  var sendRequest = require('sendRequest').sendRequest;
  var lazyBG = requireNative('lazy_background_page');

  chromeHidden.registerCustomHook('experimental.usb', function(api) {
      var apiFunctions = api.apiFunctions;

      apiFunctions.setHandleRequest('findDevice', function() {
          var args = arguments;
          if (args[2] && args[2].onEvent) {
            var id = GetNextUsbEventId();
            args[2].srcId = id;
            chromeHidden.usb.handlers[id] = args[2].onEvent;
            lazyBG.IncrementKeepaliveCount();
          }
          sendRequest(this.name, args, this.definition.parameters);
          return id;
        });

      chromeHidden.usb = {};
      chromeHidden.usb.handlers = {};
      chrome.experimental.usb.onEvent.addListener(function(event) {
          var eventHandler = chromeHidden.usb.handlers[event.srcId];
          if (eventHandler) {
            switch (event.type) {
              case 'transferComplete':
                eventHandler({resultCode: event.resultCode, data: event.data,
                    error: event.error});
                break;
              default:
                console.error('Unexpected UsbEvent, type ' + event.type);
                break;
            }
          }
        });
    });
