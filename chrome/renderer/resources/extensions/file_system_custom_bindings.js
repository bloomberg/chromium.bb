// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the fileSystem API.

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.registerCustomHook('fileSystem', function(bindingsAPI) {
  bindingsAPI.apiFunctions.setUpdateArgumentsPostValidate(
      'getDisplayPath', function(fileEntry, callback) {
    var fileSystemName = fileEntry.filesystem.name;
    var relativePath = fileEntry.fullPath.slice(1);
    return [fileSystemName, relativePath, callback];
  });
});
