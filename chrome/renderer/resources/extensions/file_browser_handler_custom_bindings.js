// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the fileBrowserHandler API.

var fileBrowserNatives = requireNative('file_browser_handler');
var GetExternalFileEntry = fileBrowserNatives.GetExternalFileEntry;

var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();

chromeHidden.Event.registerArgumentMassager('fileBrowserHandler.onExecute',
    function(args) {
  if (args.length < 2)
    return;
  var fileList = args[1].entries;
  if (!fileList)
    return;
  // The second parameter for this event's payload is file definition
  // dictionary that we used to reconstruct File API's Entry instance
  // here.
  for (var i = 0; i < fileList.length; i++)
    fileList[i] = GetExternalFileEntry(fileList[i]);
});
