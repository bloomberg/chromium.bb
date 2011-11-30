// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * TestFixture for NTP4 WebUI testing.
 * @extends {testing.Test}
 * @constructor
 */
function NTP4WebUITest() {}

NTP4WebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the newtab page & call preLoad().
   */
  browsePreload: 'chrome://newtab',
};

// Test loading new tab page and selecting each card doesn't have console
// errors.
TEST_F('NTP4WebUITest', 'TestBrowsePages', function() {
// This tests the ntp4 new tab page which is not used on touch builds.
  var cardSlider = ntp4.getCardSlider();
  assertNotEquals(null, cardSlider);
  for (var i = 0; i < cardSlider.cardCount; ++i) {
    cardSlider.selectCard(i);
    expectEquals(i, cardSlider.currentCard);
  }
});
