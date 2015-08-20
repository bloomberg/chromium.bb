// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function PowerOverlayWebUITest() {}

PowerOverlayWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://settings-frame/',

  commandLineSwitches: [{
    switchName: 'enable-power-overlay',
  }],

  /** @override */
  preLoad: function() {
    this.makeAndRegisterMockHandler([
      'updatePowerStatus',
      'setPowerSource',
    ]);
    this.mockHandler.expects(atLeastOnce()).updatePowerStatus();
  },

  /**
   * Sets power sources using a deep copy of |sources|.
   * @param {Array<Object>} sources
   * @param {string} sourceId
   * @param {bool} isUsbCharger
   * @param {bool} isCalculating
   */
  setPowerSources: function(sources, sourceId, isUsbCharger, isCalculating) {
    var sourcesCopy = sources.map(function(source) {
      return Object.assign({}, source);
    });
    options.PowerOverlay.setPowerSources(
        sourcesCopy, sourceId, isUsbCharger, isCalculating);
  },

  /**
   * Simulates the user selecting a power source, verifying that the overlay
   * calls setPowerSource.
   * @param {string} sourceId
   */
  selectPowerSource: function(sourceId) {
    this.mockHandler.expects(once()).setPowerSource(eq(sourceId));
    $('power-source-dropdown').value = sourceId;
    expectTrue(cr.dispatchSimpleEvent($('power-source-dropdown'), 'change'));
  },

  /**
   * Checks that the sources dropdown is visible.
   * @param {string} sourceId The ID of the source that should be selected.
   */
  checkSource: function(sourceId) {
    expectTrue($('power-source-charger').hidden);
    expectFalse($('power-sources').hidden);
    expectEquals(sourceId, $('power-source-dropdown').value);
  },

  checkNoSources: function() {
    expectTrue($('power-source-charger').hidden);
    expectTrue($('power-sources').hidden);
  },

  checkDedicatedCharger: function() {
    expectFalse($('power-source-charger').hidden);
    expectTrue($('power-sources').hidden);
  },
};

TEST_F('PowerOverlayWebUITest', 'testNoPowerSources', function() {
  assertEquals(this.browsePreload, document.location.href);
  this.mockHandler.expects(never()).setPowerSource();
  $('power-settings-link').click();

  // This should be the initial state.
  this.checkNoSources();

  // Setting an empty sources list shouldn't change the state.
  this.setPowerSources([], '', false, false);
  this.checkNoSources();
});

TEST_F('PowerOverlayWebUITest', 'testDedicatedCharger', function() {
  assertEquals(this.browsePreload, document.location.href);
  this.mockHandler.expects(never()).setPowerSource();
  $('power-settings-link').click();

  var fakeSources = [{
    id: 'source1',
    description: 'Left port',
    type: options.PowerStatusDeviceType.DEDICATED_CHARGER,
  }];

  this.setPowerSources(fakeSources, 'source1', false, false);
  this.checkDedicatedCharger();

  // Remove the charger.
  this.setPowerSources([], '');
  this.checkNoSources();

  // Set a low-powered charger.
  this.setPowerSources(fakeSources, 'source1', true, false);
  this.checkDedicatedCharger();
});

TEST_F('PowerOverlayWebUITest', 'testSingleSource', function() {
  assertEquals(this.browsePreload, document.location.href);
  $('power-settings-link').click();

  var fakeSources = [{
    id: 'source1',
    description: 'Left port',
    type: options.PowerStatusDeviceType.DUAL_ROLE_USB,
  }];

  this.setPowerSources(fakeSources, '', false, false);
  this.checkSource('');

  this.selectPowerSource('source1');
  this.checkSource('source1');

  // Remove the device.
  this.setPowerSources([], '', false, false);
  this.checkNoSources();
});

TEST_F('PowerOverlayWebUITest', 'testMultipleSources', function() {
  assertEquals(this.browsePreload, document.location.href);
  $('power-settings-link').click();

  var fakeSources = [{
    id: 'source1',
    description: 'Left port',
    type: options.PowerStatusDeviceType.DUAL_ROLE_USB,
  }, {
    id: 'source2',
    description: 'Right port',
    type: options.PowerStatusDeviceType.DUAL_ROLE_USB,
  }, {
    id: 'source3',
    description: 'Front port',
    type: options.PowerStatusDeviceType.DUAL_ROLE_USB,
  }, {
    id: 'source4',
    description: 'Rear port',
    type: options.PowerStatusDeviceType.DUAL_ROLE_USB,
  }];

  // Use a dual-role device.
  this.setPowerSources(fakeSources, 'source2', false, false);
  this.checkSource('source2');

  // Use a USB charger.
  this.setPowerSources(fakeSources, 'source3', true, false);
  this.checkSource('source3');

  // Remove the currently used device.
  fakeSources.splice(2, 1);
  this.setPowerSources(fakeSources, 'source4', false, false);
  this.checkSource('source4');

  // Do not charge (use battery).
  this.setPowerSources(fakeSources, '', false, false);
  this.checkSource('');

  // The user selects a device.
  this.selectPowerSource('source1');

  // The user selects the battery.
  this.selectPowerSource('');
});
