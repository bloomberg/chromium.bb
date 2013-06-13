// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var startTest = function(testDir) {
  LOG('startTest: ' + testDir);
  // load bootstrap script.
  var script = document.createElement('script');
  script.type = 'text/javascript';
  script.src = testDir + '/bootstrap.js';
  document.getElementsByTagName('head')[0].appendChild(script);
};

var onloadFunction = function() {
  window.console.log('app.onload');
  chrome.test.getConfig(function(config) {
    LOG('embeder.common got config: ' + config);
    LOG('customArg: ' + config.customArg);
    loadFired = true;
    window['startTest'](config.customArg);
  });
};

onload = onloadFunction;

