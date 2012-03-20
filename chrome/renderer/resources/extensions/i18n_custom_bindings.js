// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the i18n API.

var i18nNatives = requireNative('i18n');
var GetL10nMessage = i18nNatives.GetL10nMessage;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('i18n', function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setUpdateArgumentsPreValidate('getMessage', function() {
    var args = Array.prototype.slice.call(arguments);

    // The first argument is the message, and should be a string.
    var message = args[0];
    if (typeof(message) !== 'string') {
      console.warn(extensionId + ': the first argument to getMessage should ' +
                   'be type "string", was ' + message +
                   ' (type "' + typeof(message) + '")');
      args[0] = String(message);
    }

    return args;
  });

  apiFunctions.setHandleRequest('getMessage',
                                function(messageName, substitutions) {
    return GetL10nMessage(messageName, substitutions, extensionId);
  });
});
