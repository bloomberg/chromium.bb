// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the i18n API.

(function() {

native function GetChromeHidden();
native function GetL10nMessage();

GetChromeHidden().registerCustomHook('i18n',
                                     function(bindingsAPI, extensionId) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setHandleRequest('getMessage',
                                function(messageName, substitutions) {
    return GetL10nMessage(messageName, substitutions, extensionId);
  });
});

})();
