// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the pageAction API.

(function() {

native function GetChromeHidden();

GetChromeHidden().registerCustomHook('pageAction', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;
  var setIcon = bindingsAPI.setIcon;

  apiFunctions.setHandleRequest('pageAction.setIcon', function(details) {
    setIcon(details, this.name, this.definition.parameters, 'page action');
  });
});

})();
