// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Custom bindings for the pageCapture API.

(function() {

native function GetChromeHidden();
native function CreateBlob(filePath);
native function SendResponseAck(requestId);

var chromeHidden = GetChromeHidden();

chromeHidden.registerCustomHook('pageCapture', function(bindingsAPI) {
  var apiFunctions = bindingsAPI.apiFunctions;

  apiFunctions.setCustomCallback(
      "pageCapture.saveAsMHTML", function(name, request, response) {
    var params = chromeHidden.JSON.parse(response);
    var path = params.mhtmlFilePath;
    var size = params.mhtmlFileLength;

    if (request.callback)
      request.callback(CreateBlob(path, size));
    request.callback = null;

    // Notify the browser. Now that the blob is referenced from JavaScript,
    // the browser can drop its reference to it.
    SendResponseAck(request.id);
  });
});

})();
