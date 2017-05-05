// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /**
   * @constructor
   * @implements {settings.LanguagesBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestLanguagesBrowserProxy = function() {
    var methodNames = [];
    if (cr.isChromeOS || cr.isWindows)
      methodNames.push('getProspectiveUILanguage');

    settings.TestBrowserProxy.call(this, methodNames);

    /** @private {!LanguageSettingsPrivate} */
    this.languageSettingsPrivate_ = new settings.FakeLanguageSettingsPrivate();

    /** @private {!InputMethodPrivate} */
    this.inputMethodPrivate_ = new settings.FakeInputMethodPrivate();
  };

  TestLanguagesBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    getLanguageSettingsPrivate: function() {
      return this.languageSettingsPrivate_;
    },

    /** @override */
    getInputMethodPrivate: function() {
      return this.inputMethodPrivate_;
    },
  };

  if (cr.isChromeOS || cr.isWindows) {
    /** @override */
    TestLanguagesBrowserProxy.prototype.getProspectiveUILanguage =
        function() {
      this.methodCalled('getProspectiveUILanguage');
      return Promise.resolve('en-US');
    };
  }

  return {
    TestLanguagesBrowserProxy: TestLanguagesBrowserProxy,
  };
});
