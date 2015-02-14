// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the platformKeys API.

var binding = require('binding').Binding.create('platformKeys');
var SubtleCrypto = require('platformKeys.SubtleCrypto').SubtleCrypto;
var internalAPI = require('platformKeys.internalAPI');

binding.registerCustomHook(function(api) {
  var apiFunctions = api.apiFunctions;
  var subtleCrypto = new SubtleCrypto('' /* tokenId */);

  apiFunctions.setHandleRequest(
      'selectClientCertificates', function(details, callback) {
        internalAPI.selectClientCertificates(details, callback);
      });

  apiFunctions.setHandleRequest(
      'subtleCrypto', function() { return subtleCrypto });
});

exports.binding = binding.generate();
