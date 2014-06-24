// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test configuration used by test page to configure the player app and other
// test specific configurations.
function TestConfig() {
  this.mediaFile = null;
  this.keySystem = null;
  this.mediaType = null;
  this.licenseServerURL = null;
  this.useMSE = false;
  this.usePrefixedEME = false;
  this.runFPS = false;
}

TestConfig.prototype.loadQueryParams = function() {
  // Load query parameters and set default values.
  var r = /([^&=]+)=?([^&]*)/g;
  // Lambda function for decoding extracted match values. Replaces '+' with
  // space so decodeURIComponent functions properly.
  var decodeURI = function decodeURI(s) {
      return decodeURIComponent(s.replace(/\+/g, ' '));
  };
  var match;
  while (match = r.exec(window.location.search.substring(1)))
    this[decodeURI(match[1])] = decodeURI(match[2]);
  this.useMSE = this.useMSE == '1' || this.useMSE == 'true';
  this.usePrefixedEME =
      this.usePrefixedEME == '1' || this.usePrefixedEME == 'true';
};
