// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN_INCLUDE(['switch_access_e2e_test_base.js']);

UNDEFINED_INTERVAL_DELAY = -1;

/**
 * @constructor
 * @extends {SwitchAccessE2ETest}
 */
function SwitchAccessAutoScanManagerTest() {
  SwitchAccessE2ETest.call(this);
}

SwitchAccessAutoScanManagerTest.prototype = {
  __proto__: SwitchAccessE2ETest.prototype,

  /** @override */
  setUp() {
    AutoScanManager.instance.primaryScanTime_ = 1000;
    // Use intervalCount and intervalDelay to check how many intervals are
    // currently running (should be no more than 1) and the current delay.
    window.intervalCount = 0;
    window.intervalDelay = UNDEFINED_INTERVAL_DELAY;
    window.defaultSetInterval = window.setInterval;
    window.defaultClearInterval = window.clearInterval;
    NavigationManager.defaultMoveForward = NavigationManager.moveForward;
    NavigationManager.moveForwardCount = 0;


    window.setInterval = function(func, delay) {
      window.intervalCount++;
      window.intervalDelay = delay;

      // Override the delay for testing.
      return window.defaultSetInterval(func, 0);
    };

    window.clearInterval = function(intervalId) {
      if (intervalId) {
        window.intervalCount--;
      }
      window.defaultClearInterval(intervalId);
    };

    NavigationManager.moveForward = function() {
      NavigationManager.moveForwardCount++;
      NavigationManager.defaultMoveForward();
    };

    NavigationManager.instance.onMoveForwardForTesting_ = null;
  }
};

TEST_F('SwitchAccessAutoScanManagerTest', 'SetEnabled', function() {
  this.runWithLoadedTree('', (desktop) => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(
        0, NavigationManager.moveForwardCount,
        'Incorrect initialization of moveForwardCount');
    assertEquals(0, intervalCount, 'Incorrect initialization of intervalCount');

    NavigationManager.instance
        .onMoveForwardForTesting_ = this.newCallback(() => {
      assertTrue(
          AutoScanManager.instance.isRunning_(),
          'Auto scan manager has stopped running');
      assertGT(
          NavigationManager.moveForwardCount, 0,
          'Switch Access has not moved forward');
      assertEquals(
          1, intervalCount, 'The number of intervals is no longer exactly 1');
    });

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is not running');
    assertEquals(1, intervalCount, 'There is not exactly 1 interval');
  });
});

TEST_F('SwitchAccessAutoScanManagerTest', 'SetEnabledMultiple', function() {
  this.runWithLoadedTree('', (desktop) => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(0, intervalCount, 'Incorrect initialization of intervalCount');

    AutoScanManager.setEnabled(true);
    AutoScanManager.setEnabled(true);
    AutoScanManager.setEnabled(true);

    assertTrue(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is not running');
    assertEquals(1, intervalCount, 'There is not exactly 1 interval');
  });
});

TEST_F('SwitchAccessAutoScanManagerTest', 'EnableAndDisable', function() {
  this.runWithLoadedTree('', (desktop) => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(0, intervalCount, 'Incorrect initialization of intervalCount');

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is not running');
    assertEquals(1, intervalCount, 'There is not exactly 1 interval');

    AutoScanManager.setEnabled(false);
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager did not stop running');
    assertEquals(0, intervalCount, 'Interval was not removed');
  });
});

TEST_F(
    'SwitchAccessAutoScanManagerTest', 'RestartIfRunningMultiple', function() {
      this.runWithLoadedTree('', (desktop) => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running prematurely');
        assertEquals(
            0, NavigationManager.moveForwardCount,
            'Incorrect initialization of moveForwardCount');
        assertEquals(
            0, intervalCount, 'Incorrect initialization of intervalCount');

        AutoScanManager.setEnabled(true);
        AutoScanManager.restartIfRunning();
        AutoScanManager.restartIfRunning();
        AutoScanManager.restartIfRunning();

        assertTrue(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is not running');
        assertEquals(1, intervalCount, 'There is not exactly 1 interval');
      });
    });

TEST_F(
    'SwitchAccessAutoScanManagerTest', 'RestartIfRunningWhenOff', function() {
      this.runWithLoadedTree('', (desktop) => {
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager is running at start.');
        AutoScanManager.restartIfRunning();
        assertFalse(
            AutoScanManager.instance.isRunning_(),
            'Auto scan manager enabled by restartIfRunning');
      });
    });

TEST_F('SwitchAccessAutoScanManagerTest', 'SetPrimaryScanTime', function() {
  this.runWithLoadedTree('', (desktop) => {
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Auto scan manager is running prematurely');
    assertEquals(
        UNDEFINED_INTERVAL_DELAY, intervalDelay,
        'Interval delay improperly initialized');

    AutoScanManager.setPrimaryScanTime(2);
    assertFalse(
        AutoScanManager.instance.isRunning_(),
        'Setting default scan time started auto-scanning');
    assertEquals(
        2, AutoScanManager.instance.primaryScanTime_,
        'Default scan time set improperly');
    assertEquals(
        UNDEFINED_INTERVAL_DELAY, intervalDelay,
        'Interval delay set prematurely');

    AutoScanManager.setEnabled(true);
    assertTrue(
        AutoScanManager.instance.isRunning_(), 'Auto scan did not start');
    assertEquals(
        2, AutoScanManager.instance.primaryScanTime_,
        'Default scan time has changed');
    assertEquals(2, intervalDelay, 'Interval delay not set');

    AutoScanManager.setPrimaryScanTime(5);
    assertTrue(AutoScanManager.instance.isRunning_(), 'Auto scan stopped');
    assertEquals(
        5, AutoScanManager.instance.primaryScanTime_,
        'Default scan time did not change when set a second time');
    assertEquals(5, intervalDelay, 'Interval delay did not update');
  });
});
