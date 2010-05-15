// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Search for communication channel div.
var divOpenerHandler = document.getElementById('central_roster_opener');
if (divOpenerHandler) {
  divOpenerHandler.addEventListener(ChatBridgeEventTypes.OPEN_CENTRAL_ROSTER,
      function(event) {
        chrome.extension.sendRequest({msg: 'openCentralRoster'});
      }, false);
}
