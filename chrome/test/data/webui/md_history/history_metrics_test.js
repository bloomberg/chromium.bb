// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('md_history.history_metrics_test', function() {
  /**
   * @constructor
   * @extends {md_history.BrowserService}
   */
  var TestMetricsBrowserService = function() { this.histogramMap = {}; };

  function registerTests() {
    suite('Metrics', function() {
      var service;
      var app;

      suiteSetup(function() {
        TestMetricsBrowserService.prototype = {
          __proto__: md_history.BrowserService.prototype,

          /** @override */
          recordHistogram: function(histogram, value, max) {
            assertTrue(value < max);

            if (!(histogram in this.histogramMap))
              this.histogramMap[histogram] = {};

            if (!(value in this.histogramMap[histogram]))
              this.histogramMap[histogram][value] = 0;

            this.histogramMap[histogram][value]++;
          }
        };
      });

      setup(function() {
        md_history.BrowserService.instance_ = new TestMetricsBrowserService();
        service = md_history.BrowserService.getInstance();

        app = replaceApp();
        updateSignInState(false);
        return flush();
      });

      test('History.HistoryPageView', function() {
        app.grouped_ = true;

        var histogram = service.histogramMap['History.HistoryPageView'];
        assertEquals(1, histogram[HistoryPageViewHistogram.HISTORY]);

        app.selectedPage_ = 'syncedTabs';
        assertEquals(1, histogram[HistoryPageViewHistogram.SIGNIN_PROMO]);
        updateSignInState(true);
        return flush().then(() => {
          assertEquals(1, histogram[HistoryPageViewHistogram.SYNCED_TABS]);
          app.selectedPage_ = 'history';
          assertEquals(2, histogram[HistoryPageViewHistogram.HISTORY]);
          app.set('queryState_.range', HistoryRange.WEEK);
          assertEquals(1, histogram[HistoryPageViewHistogram.GROUPED_WEEK]);
          app.set('queryState_.range', HistoryRange.MONTH);
          assertEquals(1, histogram[HistoryPageViewHistogram.GROUPED_MONTH]);
        });
      });
    });
  }
  return {
    registerTests: registerTests
  };
});
