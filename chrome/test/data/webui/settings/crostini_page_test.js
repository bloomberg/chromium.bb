// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {?SettingsCrostiniPageElement} */
let crostiniPage = null;

/** @type {?TestCrostiniBrowserProxy} */
let crostiniBrowserProxy = null;

function setCrostiniPrefs(enabled, opt_sharedPaths, opt_sharedUsbDevices) {
  crostiniPage.prefs = {
    crostini: {
      enabled: {value: enabled},
      shared_paths: {value: opt_sharedPaths || []},
      shared_usb_devices: {value: opt_sharedUsbDevices || []},
    }
  };
  crostiniBrowserProxy.enabled = enabled;
  crostiniBrowserProxy.sharedPaths = opt_sharedPaths || [];
  crostiniBrowserProxy.sharedUsbDevices = opt_sharedUsbDevices || [];
  Polymer.dom.flush();
}

suite('CrostiniPageTests', function() {
  setup(function() {
    crostiniBrowserProxy = new TestCrostiniBrowserProxy();
    settings.CrostiniBrowserProxyImpl.instance_ = crostiniBrowserProxy;
    PolymerTest.clearBody();
    crostiniPage = document.createElement('settings-crostini-page');
    document.body.appendChild(crostiniPage);
    testing.Test.disableAnimationsAndTransitions();
  });

  teardown(function() {
    crostiniPage.remove();
  });

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      crostiniPage.async(resolve);
    });
  }

  suite('Main Page', function() {
    setup(function() {
      setCrostiniPrefs(false);
    });

    test('Enable', function() {
      const button = crostiniPage.$$('#enable');
      assertTrue(!!button);
      assertFalse(!!crostiniPage.$$('.subpage-arrow'));

      button.click();
      Polymer.dom.flush();
      setCrostiniPrefs(crostiniBrowserProxy.enabled);
      assertTrue(crostiniPage.prefs.crostini.enabled.value);

      assertTrue(!!crostiniPage.$$('.subpage-arrow'));
    });
  });

  suite('SubPageDetails', function() {
    let subpage;

    /**
     * Returns a new promise that resolves after a window 'popstate' event.
     * @return {!Promise}
     */
    function whenPopState() {
      return new Promise(function(resolve) {
        window.addEventListener('popstate', function callback() {
          window.removeEventListener('popstate', callback);
          resolve();
        });
      });
    }

    setup(function() {
      setCrostiniPrefs(true);
      settings.navigateTo(settings.routes.CROSTINI);
      crostiniPage.$$('#crostini').click();
      return flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-subpage');
        assertTrue(!!subpage);
      });
    });

    test('Sanity', function() {
      assertTrue(!!subpage.$$('#crostini-shared-paths'));
      assertTrue(!!subpage.$$('#remove'));
    });

    test('SharedPaths', function() {
      assertTrue(!!subpage.$$('#crostini-shared-paths .subpage-arrow'));
      subpage.$$('#crostini-shared-paths .subpage-arrow').click();
      return flushAsync().then(() => {
        subpage = crostiniPage.$$('settings-crostini-shared-paths');
        assertTrue(!!subpage);
      });
    });


    test('Remove', function() {
      assertTrue(!!subpage.$$('#remove .subpage-arrow'));
      subpage.$$('#remove .subpage-arrow').click();
      setCrostiniPrefs(crostiniBrowserProxy.enabled);
      assertFalse(crostiniPage.prefs.crostini.enabled.value);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.CROSTINI);
        assertTrue(!!crostiniPage.$$('#enable'));
      });
    });

    test('HideOnDisable', function() {
      assertEquals(
          settings.getCurrentRoute(), settings.routes.CROSTINI_DETAILS);
      setCrostiniPrefs(false);
      return whenPopState().then(function() {
        assertEquals(settings.getCurrentRoute(), settings.routes.CROSTINI);
      });
    });
  });

  suite('SubPageSharedPaths', function() {
    let subpage;

    setup(function() {
      setCrostiniPrefs(true, crostiniBrowserProxy.sharedPaths);
      return flushAsync().then(() => {
        settings.navigateTo(settings.routes.CROSTINI_SHARED_PATHS);
        return flushAsync().then(() => {
          subpage = crostiniPage.$$('settings-crostini-shared-paths');
          assertTrue(!!subpage);
        });
      });
    });

    test('Sanity', function() {
      assertEquals(
          2, subpage.shadowRoot.querySelectorAll('.settings-box').length);
      assertEquals(2, subpage.shadowRoot.querySelectorAll('.list-item').length);
    });

    test('Remove', function() {
      assertFalse(subpage.$.crostiniInstructionsRemove.hidden);
      assertTrue(!!subpage.$$('.list-item button'));
      // Remove first shared path, still one left.
      subpage.$$('.list-item button').click();
      assertEquals(1, crostiniBrowserProxy.sharedPaths.length);
      setCrostiniPrefs(true, crostiniBrowserProxy.sharedPaths);
      return flushAsync()
          .then(() => {
            Polymer.dom.flush();
            assertEquals(
                1, subpage.shadowRoot.querySelectorAll('.list-item').length);
            assertFalse(subpage.$.crostiniInstructionsRemove.hidden);

            // Remove remaining shared path, none left.
            subpage.$$('.list-item button').click();
            assertEquals(0, crostiniBrowserProxy.sharedPaths.length);
            setCrostiniPrefs(true, crostiniBrowserProxy.sharedPaths);
            return flushAsync();
          })
          .then(() => {
            Polymer.dom.flush();
            assertEquals(
                0, subpage.shadowRoot.querySelectorAll('.list-item').length);
            // Verify remove instructions are hidden.
            assertTrue(subpage.$.crostiniInstructionsRemove.hidden);
          });
    });
  });

  suite('SubPageSharedUsbDevices', function() {
    let subpage;

    setup(function() {
      setCrostiniPrefs(true, [], [
        {'shared': true, 'guid': '0001', 'name': 'usb_dev1'},
        {'shared': false, 'guid': '0002', 'name': 'usb_dev2'},
        {'shared': true, 'guid': '0003', 'name': 'usb_dev3'}
      ]);
      return flushAsync()
          .then(() => {
            settings.navigateTo(settings.routes.CROSTINI_SHARED_USB_DEVICES);
            return flushAsync();
          })
          .then(() => {
            subpage = crostiniPage.$$('settings-crostini-shared-usb-devices');
            assertTrue(!!subpage);
          });
    });

    test('USB devices are shown', function() {
      assertEquals(
          3, subpage.shadowRoot.querySelectorAll('.settings-box').length);
    });

    test('USB shared pref is updated by toggling', function() {
      assertTrue(!!subpage.$$('.settings-box .toggle'));
      subpage.$$('.toggle').click();
      return flushAsync()
          .then(() => {
            Polymer.dom.flush();
            assertEquals(
                crostiniBrowserProxy.sharedUsbDevices[0].shared, false);

            subpage.$$('.toggle').click();
            return flushAsync();
          })
          .then(() => {
            Polymer.dom.flush();
            assertEquals(crostiniBrowserProxy.sharedUsbDevices[0].shared, true);
          });
    });
  });
});
