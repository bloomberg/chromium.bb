// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the Display Source API.

var binding = require('binding').Binding.create('displaySource');
var chrome = requireNative('chrome').GetChrome();
var lastError = require('lastError');
var natives = requireNative('display_source');

binding.registerCustomHook(function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;
  apiFunctions.setHandleRequest('startSession',
      function(sessionInfo, callback) {
        try {
          natives.StartSession(sessionInfo);
        } catch (e) {
          lastError.set('displaySource.startSession', e.message, null, chrome);
        } finally {
           if (callback !== undefined)
             callback();
           lastError.clear(chrome);
        }
  });
  apiFunctions.setHandleRequest('terminateSession',
      function(sink_id, callback) {
        try {
          natives.TerminateSession(sink_id);
        } catch (e) {
          lastError.set(
              'displaySource.terminateSession', e.message, null, chrome);
        } finally {
           if (callback !== undefined)
             callback();
           lastError.clear(chrome);
        }
  });
});

exports.$set('binding', binding.generate());
