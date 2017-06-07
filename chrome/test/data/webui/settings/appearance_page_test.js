// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.AppearanceBrowserProxy}
 * @extends {TestBrowserProxy}
 */
var TestAppearanceBrowserProxy = function() {
  TestBrowserProxy.call(this, [
    'getDefaultZoom',
    'getThemeInfo',
    'isSupervised',
    'openWallpaperManager',
    'useDefaultTheme',
    'useSystemTheme',
    'validateStartupPage',
  ]);

  /** @private */
  this.defaultZoom_ = 1;

  /** @private */
  this.isSupervised_ = false;

  /** @private */
  this.isHomeUrlValid_ = true;
};

TestAppearanceBrowserProxy.prototype = {
  __proto__: TestBrowserProxy.prototype,

  /** @override */
  getDefaultZoom: function() {
    this.methodCalled('getDefaultZoom');
    return Promise.resolve(this.defaultZoom_);
  },

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

  /** @param {number} defaultZoom */
  setDefaultZoom: function(defaultZoom) {
    this.defaultZoom_ = defaultZoom;
  },

  /** @param {boolean} Whether the user is supervised */
  setIsSupervised: function(isSupervised) {
    this.isSupervised_ = isSupervised;
  },

  /** @override */
  validateStartupPage: function(url) {
    this.methodCalled('validateStartupPage', url);
    return Promise.resolve(this.isHomeUrlValid_);
  },

  /**
   * @param {boolean} isValid
   */
   setValidStartupPageResponse: function(isValid) {
     this.isHomeUrlValid_ = isValid;
   }
};

var appearancePage = null;

/** @type {?TestAppearanceBrowserProxy} */
var appearanceBrowserProxy = null;

function createAppearancePage() {
  appearanceBrowserProxy.reset();
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
}

suite('AppearanceHandler', function() {
  setup(function() {
    appearanceBrowserProxy = new TestAppearanceBrowserProxy();
    settings.AppearanceBrowserProxyImpl.instance_ = appearanceBrowserProxy;
    createAppearancePage();
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
      assertTrue(appearancePage.$$('#themesSecondaryActions').hidden);

      appearanceBrowserProxy.setIsSupervised(false);
      appearancePage.set(THEME_ID_PREF, 'fake theme id');
      Polymer.dom.flush();
      // If there's "USE" buttons again, the container should be visible.
      assertTrue(!!appearancePage.$$('#useDefault'));
      assertFalse(appearancePage.$$('#themesSecondaryActions').hidden);

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

  test('default zoom handling', function() {
    function getDefaultZoomText() {
      var zoomLevel = appearancePage.$.zoomLevel;
      return zoomLevel.options[zoomLevel.selectedIndex].textContent.trim();
    }

    return appearanceBrowserProxy.whenCalled('getDefaultZoom').then(function() {
      assertEquals('100%', getDefaultZoomText());

      appearanceBrowserProxy.setDefaultZoom(2 / 3);
      createAppearancePage();
      return appearanceBrowserProxy.whenCalled('getDefaultZoom');
    }).then(function() {
      assertEquals('67%', getDefaultZoomText());

      appearanceBrowserProxy.setDefaultZoom(11 / 10);
      createAppearancePage();
      return appearanceBrowserProxy.whenCalled('getDefaultZoom');
    }).then(function() {
      assertEquals('110%', getDefaultZoomText());

      appearanceBrowserProxy.setDefaultZoom(1.7499999999999);
      createAppearancePage();
      return appearanceBrowserProxy.whenCalled('getDefaultZoom');
    }).then(function() {
      assertEquals('175%', getDefaultZoomText());
    });
  });

  test('show home button toggling', function() {
    assertFalse(!!appearancePage.$$('.list-frame'));
    appearancePage.set('prefs', {browser: {show_home_button: {value: true}}});

    Polymer.dom.flush();

    assertTrue(!!appearancePage.$$('.list-frame'));
  });
});

suite('HomeUrlInput', function() {
  var homeUrlInput;

  setup(function() {
    appearanceBrowserProxy = new TestAppearanceBrowserProxy();
    settings.AppearanceBrowserProxyImpl.instance_ = appearanceBrowserProxy;
    PolymerTest.clearBody();

    homeUrlInput = document.createElement('home-url-input');
    homeUrlInput.set(
        'pref', {type: chrome.settingsPrivate.PrefType.URL, value: 'test'});

    document.body.appendChild(homeUrlInput);
    Polymer.dom.flush();
  });

  test('home button urls', function() {
    assertFalse(homeUrlInput.invalid);
    assertEquals(homeUrlInput.value, 'test');

    homeUrlInput.value = '@@@';
    appearanceBrowserProxy.setValidStartupPageResponse(false);
    homeUrlInput.$.input.fire('input');

    return appearanceBrowserProxy.whenCalled('validateStartupPage')
        .then(function(url) {
          assertEquals(homeUrlInput.value, url);
          Polymer.dom.flush();
          assertEquals(homeUrlInput.value, '@@@');  // Value hasn't changed.
          assertTrue(homeUrlInput.invalid);

          // Should reset to default value on change event.
          homeUrlInput.$.input.fire('change');
          Polymer.dom.flush();
          assertEquals(homeUrlInput.value, 'test');
        });
  });
});
