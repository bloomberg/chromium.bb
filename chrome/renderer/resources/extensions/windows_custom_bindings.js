// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the windows API.

(function() {

native function GetChromeHidden();

GetChromeHidden().registerCustomHook('windows', function(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setUpdateArgumentsPreValidate("windows.get", function() {
    // Old signature:
    //    get(int windowId, function callback);
    // New signature:
    //    get(int windowId, object populate, function callback);
    if (arguments.length == 2 && typeof(arguments[1]) == "function") {
      // If the old signature is used, add a null populate object.
      newArgs = [arguments[0], null, arguments[1]];
    } else {
      newArgs = arguments;
    }
    return newArgs;
  });

  apiFunctions.setUpdateArgumentsPreValidate("windows.getCurrent",
      function() {
    // Old signature:
    //    getCurrent(function callback);
    // New signature:
    //    getCurrent(object populate, function callback);
    if (arguments.length == 1 && typeof(arguments[0]) == "function") {
      // If the old signature is used, add a null populate object.
      newArgs = [null, arguments[0]];
    } else {
      newArgs = arguments;
    }
    return newArgs;
  });

  apiFunctions.setUpdateArgumentsPreValidate("windows.getLastFocused",
      function() {
    // Old signature:
    //    getLastFocused(function callback);
    // New signature:
    //    getLastFocused(object populate, function callback);
    if (arguments.length == 1 && typeof(arguments[0]) == "function") {
      // If the old signature is used, add a null populate object.
      newArgs = [null, arguments[0]];
    } else {
      newArgs = arguments;
    }
    return newArgs;
  });

  apiFunctions.setUpdateArgumentsPreValidate("windows.getAll", function() {
    // Old signature:
    //    getAll(function callback);
    // New signature:
    //    getAll(object populate, function callback);
    if (arguments.length == 1 && typeof(arguments[0]) == "function") {
      // If the old signature is used, add a null populate object.
      newArgs = [null, arguments[0]];
    } else {
      newArgs = arguments;
    }
    return newArgs;
  });
});

})();
