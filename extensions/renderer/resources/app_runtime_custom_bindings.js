// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the chrome.app.runtime API.

var binding = require('binding').Binding.create('app.runtime');

var AppViewInternal =
    require('binding').Binding.create('appViewInternal').generate();
var eventBindings = require('event_bindings');
var fileSystemHelpers = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;
var appNatives = requireNative('app_runtime');
var DeserializeString = appNatives.DeserializeString;
var SerializeToString = appNatives.SerializeToString;
var CreateBlob = appNatives.CreateBlob;
var entryIdManager = require('entryIdManager');

eventBindings.registerArgumentMassager('app.runtime.onEmbedRequested',
    function(args, dispatch) {
  var appEmbeddingRequest = args[0];
  var id = appEmbeddingRequest.guestInstanceId;
  delete appEmbeddingRequest.guestInstanceId;
  appEmbeddingRequest.allow = function(url) {
    AppViewInternal.attachFrame(url, id);
  };

  appEmbeddingRequest.deny = function() {
    AppViewInternal.denyRequest(id);
  };

  dispatch([appEmbeddingRequest]);
});

eventBindings.registerArgumentMassager('app.runtime.onLaunched',
    function(args, dispatch) {
  var launchData = args[0];
  if (launchData.items) {
    // An onLaunched corresponding to file_handlers in the app's manifest.
    var items = [];
    var numItems = launchData.items.length;
    var itemLoaded = function(err, item) {
      if (err) {
        console.error('Error getting fileEntry, code: ' + err.code);
      } else {
        $Array.push(items, item);
      }
      if (--numItems === 0) {
        var data = { isKioskSession: launchData.isKioskSession };
        if (items.length !== 0) {
          data.id = launchData.id;
          data.items = items;
        }
        dispatch([data]);
      }
    };
    $Array.forEach(launchData.items, function(item) {
      var fs = GetIsolatedFileSystem(item.fileSystemId);
      fs.root.getFile(item.baseName, {}, function(fileEntry) {
        entryIdManager.registerEntry(item.entryId, fileEntry);
        itemLoaded(null, { entry: fileEntry, type: item.mimeType });
      }, function(fileError) {
        itemLoaded(fileError);
      });
    });
  } else {
    // Default case. This currently covers an onLaunched corresponding to
    // url_handlers in the app's manifest.
    dispatch([launchData]);
  }
});

exports.binding = binding.generate();
