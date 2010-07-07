// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// BrowserOptions class
// Encapsulated handling of browser options page.
//
function BrowserOptions(model) {
  OptionsPage.call(this, 'browser', templateData.browserPage, 'browserPage');
}

BrowserOptions.getInstance = function() {
  if (!BrowserOptions.instance_) {
    BrowserOptions.instance_ = new BrowserOptions(null);
  }
  return BrowserOptions.instance_;
}

BrowserOptions.prototype = {
  // Inherit BrowserOptions from OptionsPage.
  __proto__: OptionsPage.prototype,

  // Initialize BrowserOptions page.
  initializePage: function() {
    // Call base class implementation to starts preference initialization.
    OptionsPage.prototype.initializePage.call(this);

    // TODO(csilv): add any needed initialization here or delete this method.
  },
};

