// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.AppearanceBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
var TestAppearanceBrowserProxy = function() {
  settings.TestBrowserProxy.call(this, [
    'getThemeInfo',
    'isSupervised',
    'openWallpaperManager',
    'useDefaultTheme',
    'useSystemTheme',
  ]);
};

TestAppearanceBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @private */
  isSupervised_: false,

  /** @override */
  getThemeInfo: function(themeId) {
    this.methodCalled('getThemeInfo', themeId);
    return Promise.resolve({name: 'Sports car red'});
  },

  /** @override */
  isSupervised: function() {
    this.methodCalled('isSupervised');
    return this.isSupervised_;
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

  /** @param {boolean} Whether the user is supervised */
  setIsSupervised: function(isSupervised) {
    this.isSupervised_ = isSupervised;
  },
};

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
            value: '',
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

  var THEME_ID_PREF = 'prefs.extensions.theme.id.value';

  if (cr.isLinux && !cr.isChromeOS) {
    var USE_SYSTEM_PREF = 'prefs.extensions.theme.use_system.value';

    test('useDefaultThemeLinux', function() {
      assertFalse(!!appearancePage.get(THEME_ID_PREF));
      assertFalse(appearancePage.get(USE_SYSTEM_PREF));
      // No custom nor system theme in use; "USE CLASSIC" should be hidden.
      assertFalse(!!appearancePage.$$('#useDefault'));

      appearancePage.set(USE_SYSTEM_PREF, true);
      Polymer.dom.flush();
      // If the system theme is in use, "USE CLASSIC" should show.
      assertTrue(!!appearancePage.$$('#useDefault'));

      appearancePage.set(USE_SYSTEM_PREF, false);
      appearancePage.set(THEME_ID_PREF, 'fake theme id');
      Polymer.dom.flush();

      // With a custom theme installed, "USE CLASSIC" should show.
      var button = appearancePage.$$('#useDefault');
      assertTrue(!!button);

      MockInteractions.tap(button);
      return appearanceBrowserProxy.whenCalled('useDefaultTheme');
    });

    test('useSystemThemeLinux', function() {
      assertFalse(!!appearancePage.get(THEME_ID_PREF));
      appearancePage.set(USE_SYSTEM_PREF, true);
      Polymer.dom.flush();
      // The "USE GTK+" button shouldn't be showing if it's already in use.
      assertFalse(!!appearancePage.$$('#useSystem'));

      appearanceBrowserProxy.setIsSupervised(true);
      appearancePage.set(USE_SYSTEM_PREF, false);
      Polymer.dom.flush();
      // Supervised users have their own theme and can't use GTK+ theme.
      assertFalse(!!appearancePage.$$('#useDefault'));
      assertFalse(!!appearancePage.$$('#useSystem'));
      // If there's no "USE" buttons, the container should be hidden.
      assertTrue(appearancePage.$$('.secondary-action').hidden);

      appearanceBrowserProxy.setIsSupervised(false);
      appearancePage.set(THEME_ID_PREF, 'fake theme id');
      Polymer.dom.flush();
      // If there's "USE" buttons again, the container should be visible.
      assertTrue(!!appearancePage.$$('#useDefault'));
      assertFalse(appearancePage.$$('.secondary-action').hidden);

      var button = appearancePage.$$('#useSystem');
      assertTrue(!!button);

      MockInteractions.tap(button);
      return appearanceBrowserProxy.whenCalled('useSystemTheme');
    });
  } else {
    test('useDefaultTheme', function() {
      assertFalse(!!appearancePage.get(THEME_ID_PREF));
      assertFalse(!!appearancePage.$$('#useDefault'));

      appearancePage.set(THEME_ID_PREF, 'fake theme id');
      Polymer.dom.flush();

      // With a custom theme installed, "RESET TO DEFAULT" should show.
      var button = appearancePage.$$('#useDefault');
      assertTrue(!!button);

      MockInteractions.tap(button);
      return appearanceBrowserProxy.whenCalled('useDefaultTheme');
    });
  }
});
