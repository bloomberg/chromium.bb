// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the experimental.socket API.

(function() {
  native function GetChromeHidden();
  native function GetNextSocketEventId();

  var chromeHidden = GetChromeHidden();

  chromeHidden.registerCustomHook('experimental.socket', function(api) {
      var apiFunctions = api.apiFunctions;
      var sendRequest = api.sendRequest;

      apiFunctions.setHandleRequest("experimental.socket.create", function() {
          var args = arguments;
          if (args.length > 1 && args[1] && args[1].onEvent) {
            var id = GetNextSocketEventId();
            args[1].srcId = id;
            chromeHidden.socket.handlers[id] = args[1].onEvent;
          }
          sendRequest(this.name, args, this.definition.parameters);
          return id;
        });

      // Setup events.
      chromeHidden.socket = {};
      chromeHidden.socket.handlers = {};
      chrome.experimental.socket.onEvent.addListener(function(event) {
          var eventHandler = chromeHidden.socket.handlers[event.srcId];
          if (eventHandler) {
            if (event.type == "writeComplete") {
              eventHandler({
               type: event.type,
                      resultCode: event.resultCode,
                      });
            } else if (event.type == "dataRead") {
              eventHandler({
               type: event.type,
                      resultCode: event.resultCode,
                      data: event.data,
                      });
            } else {
              console.error("Unexpected SocketEvent, type " + event.type);
            }
            if (event.isFinalEvent) {
              delete chromeHidden.socket.handlers[event.srcId];
            }
          }
        });
    });

})();
