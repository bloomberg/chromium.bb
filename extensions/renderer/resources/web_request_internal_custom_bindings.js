// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the webRequestInternal API.

var binding = apiBridge ||
              require('binding').Binding.create('webRequestInternal');
var sendRequest = bindingUtil ?
    $Function.bind(bindingUtil.sendRequest, bindingUtil) :
    require('sendRequest').sendRequest;

binding.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;

  apiFunctions.setHandleRequest('addEventListener', function() {
    var args = $Array.slice(arguments);
    sendRequest('webRequestInternal.addEventListener', args,
                bindingUtil ? undefined : this.definition.parameters,
                {__proto__: null, forIOThread: true});
  });

  apiFunctions.setHandleRequest('eventHandled', function() {
    var args = $Array.slice(arguments);
    sendRequest('webRequestInternal.eventHandled', args,
                bindingUtil ? undefined : this.definition.parameters,
                {__proto__: null, forIOThread: true});
  });
});

if (!apiBridge)
  exports.$set('binding', binding.generate());
