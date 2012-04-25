// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the experimental.socket API.

  var experimentalSocketNatives = requireNative('experimental_socket');
  var GetNextSocketEventId = experimentalSocketNatives.GetNextSocketEventId;

  var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
  var sendRequest = require('sendRequest').sendRequest;
  var lazyBG = requireNative('lazy_background_page');

  chromeHidden.registerCustomHook('experimental.socket', function(api) {
      var apiFunctions = api.apiFunctions;

      apiFunctions.setHandleRequest('create', function() {
          var args = arguments;
          if (args.length > 1 && args[1] && args[1].onEvent) {
            var id = GetNextSocketEventId();
            args[1].srcId = id;
            chromeHidden.socket.handlers[id] = args[1].onEvent;

            // Keep the page alive until the event finishes.
            // Balanced in eventHandler.
            lazyBG.IncrementKeepaliveCount();
          }
          sendRequest(this.name, args, this.definition.parameters);
          return id;
        });

      // Set up events.
      chromeHidden.socket = {};
      chromeHidden.socket.handlers = {};
      chrome.experimental.socket.onEvent.addListener(function(event) {
          var eventHandler = chromeHidden.socket.handlers[event.srcId];
          if (eventHandler) {
            switch (event.type) {
              case 'writeComplete':
              case 'connectComplete':
                eventHandler({
                 type: event.type,
                        resultCode: event.resultCode,
                        });
              break;
              case 'dataRead':
                eventHandler({
                 type: event.type,
                        resultCode: event.resultCode,
                        data: event.data,
                        });
                break;
              default:
                console.error('Unexpected SocketEvent, type ' + event.type);
              break;
            }
            if (event.isFinalEvent) {
              delete chromeHidden.socket.handlers[event.srcId];
              // Balanced in 'create' handler.
              lazyBG.DecrementKeepaliveCount();
            }
          }
        });
    });
