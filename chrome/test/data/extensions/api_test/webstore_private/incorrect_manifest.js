// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Parse the manifest, and mangle it via "alterManifest" (which should already
// be defined before running this file).
var manifestObj = JSON.parse(getManifest());
var manifest = alterManifest(manifestObj);

// Now cause the install to proceed - the C++ code will verify
// that we get an install error after unpacking the crx file because the crx
// file's manifest won't match what we provided for the confirmation dialog
// here.
chrome.webstorePrivate.beginInstallWithManifest2(
    { 'id': extensionId, 'manifest': manifest },
    function(result) {
  assertNoLastError();
  assertEq("", result);
  chrome.webstorePrivate.completeInstall(extensionId,
                                         function(){
    assertNoLastError();
    succeed();
   });
});
