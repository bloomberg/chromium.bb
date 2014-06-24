// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File IO test player is used to test File IO CDM functionality.
function FileIOTestPlayer(video, testConfig) {
  this.video = video;
  this.testConfig = testConfig;
}

FileIOTestPlayer.prototype.init = function() {
  PlayerUtils.initPrefixedEMEPlayer(this);
};

FileIOTestPlayer.prototype.registerEventListeners = function() {
  PlayerUtils.registerPrefixedEMEEventListeners(this);
};

FileIOTestPlayer.prototype.onWebkitKeyMessage = function(message) {
  // The test result is either '0' or '1' appended to the header.
  if (Utils.hasPrefix(message.message, FILE_IO_TEST_RESULT_HEADER)) {
    if (message.message.length != FILE_IO_TEST_RESULT_HEADER.length + 1) {
      Utils.failTest('Unexpected FileIOTest CDM message' + message.message);
      return;
    }
    var result_index = FILE_IO_TEST_RESULT_HEADER.length;
    var success = String.fromCharCode(message.message[result_index]) == 1;
    Utils.timeLog('CDM file IO test: ' + (success ? 'Success' : 'Fail'));
    if (success)
      Utils.setResultInTitle(FILE_IO_TEST_SUCCESS);
    else
      Utils.failTest('File IO CDM message fail status.');
  }
};
