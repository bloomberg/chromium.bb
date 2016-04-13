// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs polymer appearance font settings elements. */

cr.define('settings_appearance', function() {
  /**
   * A test version of FontsBrowserProxy.
   *
   * @constructor
   * @implements {settings.FontsBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestFontsBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'fetchFontsData',
      'observeAdvancedFontExtensionAvailable',
    ]);

    /** @private {!FontsData} */
    this.fontsData_ = {
      'fontList': [['font name', 'alternate', 'ltr']],
      'encodingList': [['encoding name', 'alternate', 'ltr']],
    };
  };

  TestFontsBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    fetchFontsData: function() {
      this.methodCalled('fetchFontsData');
      return Promise.resolve(this.fontsData_);
    },

    /** @override */
    observeAdvancedFontExtensionAvailable: function() {
      this.methodCalled('observeAdvancedFontExtensionAvailable');
    },
  };

  function registerAppearanceFontSettingsBrowserTest() {
    var fontsPage = null;

    /** @type {?TestFontsBrowserProxy} */
    var fontsBrowserProxy = null;

    suite('AppearanceFontHandler', function() {
      setup(function() {
        fontsBrowserProxy = new TestFontsBrowserProxy();
        settings.FontsBrowserProxyImpl.instance_ = fontsBrowserProxy;

        PolymerTest.clearBody();

        fontsPage = document.createElement('settings-appearance-fonts-page');
        document.body.appendChild(fontsPage);
      });

      teardown(function() { fontsPage.remove(); });

      test('fetchFontsData', function() {
        return fontsBrowserProxy.whenCalled('fetchFontsData');
      });
    });
  }

  return {
    registerTests: function() {
      registerAppearanceFontSettingsBrowserTest();
    },
  };
});
