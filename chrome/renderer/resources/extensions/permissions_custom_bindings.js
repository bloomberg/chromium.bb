// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the Permissions API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var sendRequest = require('sendRequest').sendRequest;
var lastError = require('lastError');

chromeHidden.registerCustomHook('permissions', function(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setUpdateArgumentsPreValidate('request',
      function() {
        if (arguments.length < 1)
          return arguments;

        var args = arguments[0].permissions;
        if (!args)
          return arguments;

        for (var i = 0; i < args.length; i += 1) {
          if (typeof(args[i]) == 'object') {
            var a = args[i];
            var keys = Object.keys(a);
            if (keys.length != 1) {
              throw new Error("Too many keys in object-style permission.");
            }
            arguments[0].permissions[i] = keys[0] + '|' +
                JSON.stringify(a[keys[0]]);
          }
        }

        return arguments;
      });
});
