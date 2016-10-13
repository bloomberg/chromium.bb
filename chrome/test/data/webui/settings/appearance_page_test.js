// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs polymer appearance font settings elements. */

cr.define('settings_appearance', function() {
  /**
   * A test version of AppearanceBrowserProxy.
   *
   * @constructor
   * @implements {settings.AppearanceBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestAppearanceBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'getThemeInfo',
      'openWallpaperManager',
      'useDefaultTheme',
      'useSystemTheme',
    ]);
  };

  TestAppearanceBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    getThemeInfo: function(themeId) {
      this.methodCalled('getThemeInfo', themeId);
      return Promise.resolve({name: 'Sports car red'});
    },

    /** @override */
    openWallpaperManager: function() {
      this.methodCalled('openWallpaperManager');
    },

    /** @override */
    useDefaultTheme: function() {
      this.methodCalled('useDefaultTheme');
    },

    /** @override */
    useSystemTheme: function() {
      this.methodCalled('useSystemTheme');
    },
  };

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
      'openAdvancedFontSettings',
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

    /** @override */
    openAdvancedFontSettings: function() {
      this.methodCalled('openAdvancedFontSettings');
    },
  };

  function registerAppearanceSettingsBrowserTest() {
    var appearancePage = null;

    /** @type {?TestAppearanceBrowserProxy} */
    var appearanceBrowserProxy = null;

    suite('AppearanceHandler', function() {
      setup(function() {
        appearanceBrowserProxy = new TestAppearanceBrowserProxy();
        settings.AppearanceBrowserProxyImpl.instance_ = appearanceBrowserProxy;

        PolymerTest.clearBody();

        appearancePage = document.createElement('settings-appearance-page');
        appearancePage.set('prefs', {
          extensions: {
            theme: {
              id: {
                value: 'asdf',
              },
              use_system: {
                value: false,
              },
            },
          },
        });
        document.body.appendChild(appearancePage);
        Polymer.dom.flush();
      });

      teardown(function() { appearancePage.remove(); });

      if (cr.isChromeOS) {
        test('wallpaperManager', function() {
          var button = appearancePage.$.wallpaperButton;
          assertTrue(!!button);
          MockInteractions.tap(button);
          return appearanceBrowserProxy.whenCalled('openWallpaperManager');
        });
      } else {
        test('noWallpaperManager', function() {
          // The wallpaper button should not be present.
          var button = appearancePage.$.wallpaperButton;
          assertFalse(!!button);
        });
      }

      test('useDefaultTheme', function() {
        var button = appearancePage.$$('#useDefault');
        assertTrue(!!button);
        MockInteractions.tap(button);
        return appearanceBrowserProxy.whenCalled('useDefaultTheme');
      });

      if (cr.isLinux && !cr.isChromeOS) {
        test('useSystemTheme', function() {
          var button = appearancePage.$$('#useSystem');
          assertTrue(!!button);
          MockInteractions.tap(button);
          return appearanceBrowserProxy.whenCalled('useSystemTheme');
        });
      }
    });
  }

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

      test('openAdvancedFontSettings', function() {
        cr.webUIListenerCallback('advanced-font-settings-installed', [true]);
        Polymer.dom.flush();
        var button = fontsPage.$$('#advancedButton');
        assert(!!button);
        MockInteractions.tap(button);
        return fontsBrowserProxy.whenCalled('openAdvancedFontSettings');
      });
    });
  }

  return {
    registerTests: function() {
      registerAppearanceFontSettingsBrowserTest();
      registerAppearanceSettingsBrowserTest();
    },
  };
});
