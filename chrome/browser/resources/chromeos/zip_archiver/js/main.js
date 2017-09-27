// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function init() {
  chrome.runtime.getBackgroundPage(function(backgroundPage) {
    backgroundPage.unpacker.app.stringDataLoadedPromise.then(function(strings) {
      loadTimeData.data = strings;
      i18nTemplate.process(document, loadTimeData);
    });
  });
}

document.addEventListener('DOMContentLoaded', init);
