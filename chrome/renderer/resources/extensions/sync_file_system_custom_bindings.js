// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom binding for the syncFileSystem API.

var binding = require('binding').Binding.create('syncFileSystem');

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();
var fileSystemNatives = requireNative('file_system_natives');
var forEach = require('utils').forEach;
var syncFileSystemNatives = requireNative('sync_file_system');

binding.registerCustomHook(function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  // Functions which take in an [instanceOf=FileEntry].
  function bindFileEntryFunction(i, functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(entry, callback) {
      var fileSystemUrl = entry.toURL();
      return [fileSystemUrl, callback];
    });
  }
  forEach(['getFileStatus'], bindFileEntryFunction);

  // Functions which take in an [instanceOf=EntryArray].
  function bindFileEntryArrayFunction(i, functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(entries, callback) {
      var fileSystemUrlArray = [];
      for (var i=0; i < entries.length; i++) {
        fileSystemUrlArray.push(entries[i].toURL());
      }
      return [fileSystemUrlArray, callback];
    });
  }
  forEach(['getFileStatuses'], bindFileEntryArrayFunction);

  // Functions which take in an [instanceOf=DOMFileSystem].
  function bindFileSystemFunction(i, functionName) {
    apiFunctions.setUpdateArgumentsPostValidate(
        functionName, function(filesystem, callback) {
      var fileSystemUrl = filesystem.root.toURL();
      return [fileSystemUrl, callback];
    });
  }
  forEach(['getUsageAndQuota'], bindFileSystemFunction);

  // Functions which return an [instanceOf=DOMFileSystem].
  apiFunctions.setCustomCallback('requestFileSystem',
                                 function(name, request, response) {
    var result = null;
    if (response) {
      result = syncFileSystemNatives.GetSyncFileSystemObject(
          response.name, response.root);
    }
    if (request.callback)
      request.callback(result);
    request.callback = null;
  });
});

chromeHidden.Event.registerArgumentMassager(
    'syncFileSystem.onFileStatusChanged', function(args, dispatch) {
  // Make FileEntry object using all the base string fields.
  var fileSystemType = args[0];
  var fileSystemName = args[1];
  var rootUrl = args[2];
  var filePath = args[3];
  var fileEntry = fileSystemNatives.GetFileEntry(fileSystemType,
      fileSystemName, rootUrl, filePath, false);

  // Combine into a single dictionary.
  var fileInfo = new Object();
  fileInfo.fileEntry = fileEntry;
  fileInfo.status = args[4];
  if (fileInfo.status == "synced") {
    fileInfo.action = args[5];
    fileInfo.direction = args[6];
  }
  dispatch([fileInfo]);
});

exports.binding = binding.generate();
