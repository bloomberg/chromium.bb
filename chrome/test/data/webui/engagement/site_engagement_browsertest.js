// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Test suite for the Site Engagement WebUI.
 */
var ROOT_PATH = '../../../../../';
var EXAMPLE_URL_1 = 'http://example.com/';
var EXAMPLE_URL_2 = 'http://shmlexample.com/';

GEN('#include "chrome/browser/engagement/site_engagement_service.h"');
GEN('#include "chrome/browser/engagement/site_engagement_service_factory.h"');
GEN('#include "chrome/browser/ui/browser.h"');

function SiteEngagementBrowserTest() {}

SiteEngagementBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://site-engagement',

  runAccessibilityChecks: false,

  isAsync: true,

  testGenPreamble: function() {
    GEN('SiteEngagementService* service =');
    GEN('  SiteEngagementServiceFactory::GetForProfile(browser()->profile());');
    GEN('service->ResetBaseScoreForURL(GURL("' + EXAMPLE_URL_1 + '"), 10);');
    GEN('service->ResetBaseScoreForURL(GURL("' + EXAMPLE_URL_2 +
        '"), 3.14159);');
  },

  extraLibraries: [
    ROOT_PATH + 'third_party/mocha/mocha.js',
    ROOT_PATH + 'chrome/test/data/webui/mocha_adapter.js',
  ],

  /** @override */
  setUp: function() {
    testing.Test.prototype.setUp.call(this);
    suiteSetup(function() {
      return whenPageIsPopulatedForTest();
    });
  },
};

// TODO(crbug.com/734716): This test is flaky.
TEST_F('SiteEngagementBrowserTest', 'DISABLED_All', function() {
  test('check engagement values are loaded', function() {
    var originCells =
        Array.from(document.getElementsByClassName('origin-cell'));
    assertDeepEquals(
        [EXAMPLE_URL_1, EXAMPLE_URL_2], originCells.map(x => x.textContent));
  });

  test('scores rounded to 2 decimal places', function() {
    var scoreInputs =
        Array.from(document.getElementsByClassName('base-score-input'));
    assertDeepEquals(['10', '3.14'], scoreInputs.map(x => x.value));
    var bonusScoreCells =
        Array.from(document.getElementsByClassName('bonus-score-cell'));
    assertDeepEquals(['0', '0'], bonusScoreCells.map(x => x.textContent));
    var totalScoreCells =
        Array.from(document.getElementsByClassName('total-score-cell'));
    assertDeepEquals(['10', '3.14'], totalScoreCells.map(x => x.textContent));
  });
  mocha.run();
});
