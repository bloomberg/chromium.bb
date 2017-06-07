// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.FontsBrowserProxy}
 * @extends {TestBrowserProxy}
 */
var TestFontsBrowserProxy = function() {
  TestBrowserProxy.call(this, [
    'fetchFontsData',
    'observeAdvancedFontExtensionAvailable',
    'openAdvancedFontSettings',
  ]);

  /** @private {!FontsData} */
  this.fontsData_ = {
    'fontList': [['font name', 'alternate', 'ltr']],
    'encodingList': [['encoding name', 'alternate', 'ltr']],
  };
};

TestFontsBrowserProxy.prototype = {
  __proto__: TestBrowserProxy.prototype,

  /** @override */
  fetchFontsData: function() {
    this.methodCalled('fetchFontsData');
    return Promise.resolve(this.fontsData_);
  },

  /** @override */
  observeAdvancedFontExtensionAvailable: function() {
    this.methodCalled('observeAdvancedFontExtensionAvailable');
  },

  /** @override */
  openAdvancedFontSettings: function() {
    this.methodCalled('openAdvancedFontSettings');
  },
};

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

  test('openAdvancedFontSettings', function() {
    cr.webUIListenerCallback('advanced-font-settings-installed', [true]);
    Polymer.dom.flush();
    var button = fontsPage.$$('#advancedButton');
    assert(!!button);
    MockInteractions.tap(button);
    return fontsBrowserProxy.whenCalled('openAdvancedFontSettings');
  });
});
