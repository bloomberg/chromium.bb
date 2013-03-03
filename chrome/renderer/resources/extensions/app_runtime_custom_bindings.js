// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the chrome.app.runtime API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var fileSystemHelpers = requireNative('file_system_natives');
var GetIsolatedFileSystem = fileSystemHelpers.GetIsolatedFileSystem;
var appNatives = requireNative('app_runtime');
var DeserializeString = appNatives.DeserializeString;
var SerializeToString = appNatives.SerializeToString;
var CreateBlob = appNatives.CreateBlob;

chromeHidden.Event.registerArgumentMassager('app.runtime.onLaunched',
    function(args, dispatch) {
  var launchData = args[0];

  if (launchData && typeof launchData.id !== 'undefined') {
    // new-style dispatch.
    var items = []
    var numItems = launchData.items.length;
    var itemLoaded = function(err, item) {
      if (err) {
        console.error('Error getting fileEntry, code: ' + err.code);
      } else {
        items.push(item);
      }
      if (--numItems === 0) {
        if (items.length === 0) {
          dispatch([]);
        } else {
          var data = { id: launchData.id, items: items };
          dispatch([data]);
        }
      }
    };
    launchData.items.forEach(function(item) {
      var fs = GetIsolatedFileSystem(item.fileSystemId);
      fs.root.getFile(item.baseName, {}, function(fileEntry) {
        itemLoaded(null, { entry: fileEntry, type: item.mimeType });
      }, function(fileError) {
        itemLoaded(fileError);
      });
    });
  } else if (launchData) {
    dispatch([launchData]);
  } else {
    dispatch([]);
  }
});
