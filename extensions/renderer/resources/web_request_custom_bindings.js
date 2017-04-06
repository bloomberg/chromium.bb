// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webRequest API.

var binding = apiBridge || require('binding').Binding.create('webRequest');
var sendRequest = bindingUtil ?
    $Function.bind(bindingUtil.sendRequest, bindingUtil) :
    require('sendRequest').sendRequest;

binding.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('handlerBehaviorChanged', function() {
    var args = $Array.slice(arguments);
    sendRequest(this.name, args, this.definition.parameters,
                {__proto__: null, forIOThread: true});
  });
});

if (!apiBridge) {
  var webRequestEvent = require('webRequestEvent').WebRequestEvent;
  binding.registerCustomEvent(webRequestEvent);
  exports.$set('binding', binding.generate());
}
