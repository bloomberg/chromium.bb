// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {string} Path to source root. */
const ROOT_PATH = '../../../../';

GEN_INCLUDE(
    [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);
GEN('#include "chromeos/constants/chromeos_features.h"');

/**
 * MdSetTimeBrowserTest tests loading and interacting with the Material Design
 * version of the "Set Time" web UI, which is normally shown as a dialog.
 * @constructor
 * @extends {PolymerTest}
 */
function MdSetTimeBrowserTest() {}

MdSetTimeBrowserTest.prototype = {
  __proto__: PolymerTest.prototype,

  browsePreload: 'chrome://set-time/',

  featureList: ['chromeos::features::kSetTimeDialogMd', ''],

  extraLibraries: PolymerTest.getLibraries(ROOT_PATH).concat([
    ROOT_PATH + 'chrome/test/data/webui/test_browser_proxy.js',
  ]),
};

TEST_F('MdSetTimeBrowserTest', 'All', function() {
  suite('SetTime', function() {
    let setTimeElement = null;
    let testBrowserProxy = null;

    /** @implements {settime.SetTimeBrowserProxy} */
    class TestSetTimeBrowserProxy extends TestBrowserProxy {
      constructor() {
        super([
          'sendPageReady',
          'setTimeInSeconds',
          'setTimezone',
          'dialogClose',
        ]);
      }

      /** @override */
      sendPageReady() {
        this.methodCalled('sendPageReady');
      }

      /** @override */
      setTimeInSeconds(timeInSeconds) {
        this.methodCalled('setTimeInSeconds', timeInSeconds);
      }

      /** @override */
      setTimezone(timezone) {
        this.methodCalled('setTimezone');
      }

      /** @override */
      dialogClose() {
        this.methodCalled('dialogClose');
      }
    }

    setup(function() {
      testBrowserProxy = new TestSetTimeBrowserProxy();
      settime.SetTimeBrowserProxyImpl.instance_ = testBrowserProxy;
      PolymerTest.clearBody();
      setTimeElement = document.createElement('set-time');
      document.body.appendChild(setTimeElement);
      Polymer.dom.flush();
    });

    teardown(function() {
      setTimeElement.remove();
    });

    test('PageReady', () => {
      // Verify the page sends the ready message.
      assertEquals(1, testBrowserProxy.getCallCount('sendPageReady'));
    });

    test('DateRangeContainsNow', () => {
      const dateInput = setTimeElement.$$('#dateInput');

      // Input element attributes min and max are strings like '2019-03-01'.
      const minDate = new Date(dateInput.min);
      const maxDate = new Date(dateInput.max);
      const now = new Date();

      // Verify min <= now <= max.
      assertLE(minDate, now);
      assertLE(now, maxDate);
    });

    test('SetDate', () => {
      const dateInput = setTimeElement.$$('#dateInput');
      assertTrue(!!dateInput);

      // Simulate the user changing the date picker forward by a week.
      const today = dateInput.valueAsDate;
      const nextWeek = new Date(today.getTime() + 7 * 24 * 60 * 60 * 1000);
      dateInput.focus();
      dateInput.valueAsDate = nextWeek;
      dateInput.blur();

      // Verify the page sends a request to move time forward.
      return testBrowserProxy.whenCalled('setTimeInSeconds')
          .then(timeInSeconds => {
            const todaySeconds = today.getTime() / 1000;
            // The exact value isn't important (it depends on the current time).
            assertGT(timeInSeconds, todaySeconds);
          });
    });

    test('SystemTimezoneChanged', () => {
      const timezoneSelect = setTimeElement.$$('#timezoneSelect');
      assertTrue(!!timezoneSelect);

      cr.webUIListenerCallback('system-timezone-changed', 'America/New_York');
      expectEquals('America/New_York', timezoneSelect.value);

      cr.webUIListenerCallback('system-timezone-changed', 'Europe/Moscow');
      expectEquals('Europe/Moscow', timezoneSelect.value);
    });
  });

  mocha.run();
});
