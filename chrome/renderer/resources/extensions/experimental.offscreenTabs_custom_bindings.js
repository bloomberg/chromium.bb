// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the experimental offscreenTabs API.

(function() {

native function GetChromeHidden();

GetChromeHidden().registerCustomHook(
    'experimental.offscreenTabs', function(api) {
  var apiFunctions = api.apiFunctions;

  function maybeCopy(src, prop, dest) {
    if (src[prop] !== undefined)
      dest[prop] = src[prop];
  };

  function keyboardEventFilter(e) {
    var result = {
      type: e.type,
      ctrlKey: e.ctrlKey,
      shiftKey: e.shiftKey,
      altKey: e.altKey,
      metaKey: e.metaKey,
    };
    maybeCopy(e, 'keyCode', result);
    maybeCopy(e, 'charCode', result);
    return result;
  };

  function mouseEventFilter(e) {
    var result = {
      type: e.type,
      ctrlKey: e.ctrlKey,
      shiftKey: e.shiftKey,
      altKey: e.altKey,
      metaKey: e.metaKey,
      button: e.button,
    };
    maybeCopy(e, 'wheelDeltaX', result);
    maybeCopy(e, 'wheelDeltaY', result);
    return result;
  };

  // We are making a copy of |arr|, but applying |func| to index 1.
  function validate(arr, func) {
    var newArr = [];
    for (var i = 0; i < arr.length; i++)
      newArr.push(i == 1 && typeof(arr) == 'object' ? func(arr[i]) : arr[i]);
    return newArr;
  }

  apiFunctions.setUpdateArgumentsPreValidate(
      'sendKeyboardEvent',
      function() { return validate(arguments, keyboardEventFilter); });
  apiFunctions.setUpdateArgumentsPreValidate(
      'sendMouseEvent',
      function() { return validate(arguments, mouseEventFilter); });
});

})();
