// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Global test configuration used by media players.  On each test run call
// TestConfig.init() to load new test configuration from the test page.
var TestConfig = new function() {
  this.mediaFile = null;
  this.keySystem = null;
  this.mediaType = null;
  this.licenseServer = null;
  this.useSRC = false;
  this.usePrefixedEME = false;
}

TestConfig.updateDocument = function() {
  Utils.addOptions(KEYSYSTEM_ELEMENT_ID, KEY_SYSTEMS);
  Utils.addOptions(MEDIA_TYPE_ELEMENT_ID, MEDIA_TYPES);
  Utils.addOptions(USE_PREFIXED_EME_ID, EME_VERSIONS_OPTIONS);

  // Load query parameters and set default values.
  var r = /([^&=]+)=?([^&]*)/g;
  // Lambda function for decoding extracted match values. Replaces '+' with
  // space so decodeURIComponent functions properly.
  var decodeURI = function decodeURI(s) {
      return decodeURIComponent(s.replace(/\+/g, ' '));
  };
  var match;
  var params = {};
  while (match = r.exec(window.location.search.substring(1)))
    params[decodeURI(match[1])] = decodeURI(match[2]);

  document.getElementById(MEDIA_FILE_ELEMENT_ID).value =
      params.mediaFile || DEFAULT_MEDIA_FILE;

  document.getElementById(LICENSE_SERVER_ELEMENT_ID).value =
      params.licenseServer || DEFAULT_LICENSE_SERVER;

  if (params.keySystem)
    document.getElementById(KEYSYSTEM_ELEMENT_ID).value = params.keySystem;
  if (params.mediaType)
    document.getElementById(MEDIA_TYPE_ELEMENT_ID).value = params.mediaType;
  if (params.useSRC) {
    params.useSRC = params.useSRC == '1' || params.useSRC == 'true';
    document.getElementById(USE_SRC_ELEMENT_ID).value = params.useSRC;
  }
  if (params.usePrefixedEME) {
    params.usePrefixedEME =
        params.usePrefixedEME == '1' || params.usePrefixedEME == 'true';
    document.getElementById(USE_PREFIXED_EME_ID).value = params.usePrefixedEME;
  }
};

TestConfig.init = function() {
  // Reload test configuration from document.
  this.mediaFile = document.getElementById(MEDIA_FILE_ELEMENT_ID).value;
  this.keySystem = document.getElementById(KEYSYSTEM_ELEMENT_ID).value;
  this.mediaType = document.getElementById(MEDIA_TYPE_ELEMENT_ID).value;
  this.useSRC = document.getElementById(USE_SRC_ELEMENT_ID).value == 'true';
  this.usePrefixedEME =
      document.getElementById(USE_PREFIXED_EME_ID).value == 'true';
  this.licenseServer = document.getElementById(LICENSE_SERVER_ELEMENT_ID).value;
};
