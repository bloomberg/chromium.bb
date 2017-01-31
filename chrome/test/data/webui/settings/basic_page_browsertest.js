// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for the Settings basic page. */

GEN_INCLUDE(['settings_page_browsertest.js']);

/**
 * @constructor
 * @extends {SettingsPageBrowserTest}
 */
function SettingsBasicPageBrowserTest() {}

SettingsBasicPageBrowserTest.prototype = {
  __proto__: SettingsPageBrowserTest.prototype,

  /** @override */
  extraLibraries: SettingsPageBrowserTest.prototype.extraLibraries.concat([
    'test_browser_proxy.js',
  ]),
};

// Times out on debug builders and may time out on memory bots because
// the Settings page can take several seconds to load in a Release build
// and several times that in a Debug build. See https://crbug.com/558434.
GEN('#if defined(MEMORY_SANITIZER) || !defined(NDEBUG)');
GEN('#define MAYBE_Load DISABLED_Load');
GEN('#else');
GEN('#define MAYBE_Load Load');
GEN('#endif');

TEST_F('SettingsBasicPageBrowserTest', 'MAYBE_Load', function() {
  // Assign |self| to |this| instead of binding since 'this' in suite()
  // and test() will be a Mocha 'Suite' or 'Test' instance.
  var self = this;

  /**
   * This fake SearchManager just hides and re-displays the sections on search.
   *
   * @implements {SearchManager}
   * @extends {settings.TestBrowserProxy}
   */
  var TestSearchManager = function() {
    settings.TestBrowserProxy.call(this, [
      'search',
    ]);

    /** @private {?settings.SearchRequest} */
    this.searchRequest_ = null;
  }

  TestSearchManager.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    search: function(text, page) {
      if (this.searchRequest_ == null || !this.searchRequest_.isSame(text)) {
        this.searchRequest_ = new settings.SearchRequest(text);
        this.searchRequest_.finished = true;
        this.searchRequest_.updateMatches(false);

        // The search much be resolved asynchronously just like the real
        // SearchManager. Otherwise, the race condition doesn't manifest.
        setTimeout(function() {
          // All the sections must be hidden by the TestSearchManager, just like
          // the real SearchManager. Otherwise, the bug doesn't manifest.
          var sections =
              Polymer.dom().querySelectorAll('* /deep/ settings-section');
          for (var i = 0; i < sections.length; i++)
            sections[i].hiddenBySearch = !!text;

          this.searchRequest_.resolver.resolve(this.searchRequest_);
          this.methodCalled('search', text);
        }.bind(this), 0);
      }
      return this.searchRequest_.resolver.promise;
    },
  };

  // Register mocha tests.
  suite('SettingsPage', function() {
    test('load page', function() {
      // This will fail if there are any asserts or errors in the Settings page.
    });

    test('basic pages', function() {
      var page = self.getPage('basic');
      var sections = ['appearance', 'onStartup', 'people', 'search'];
      expectTrue(!!self.getSection(page, 'appearance'));
      if (!cr.isChromeOS)
        sections.push('defaultBrowser');
      else
        sections = sections.concat(['internet', 'bluetooth', 'device']);

      for (var i = 0; i < sections.length; i++) {
        var section = self.getSection(page, sections[i]);
        expectTrue(!!section);
        self.verifySubpagesHidden(section);
      }
    });

    test('scroll to section', function() {
      // Setting the page and section will cause a scrollToSection_.
      settings.navigateTo(settings.Route.ON_STARTUP);

      var page = self.getPage('basic');

      return new Promise(function(resolve, reject) {
        // This test checks for a regression that occurred with scrollToSection_
        // failing to find its host element.
        var intervalId = window.setInterval(function() {
          var page = self.getPage('basic');
          if (self.getSection(page, settings.getCurrentRoute().section)) {
            window.clearInterval(intervalId);
            resolve();
          }
        }, 55);
      }.bind(self)).then(function() {
        // Should be scrolled to the On Startup section.
        assertNotEquals(0, page.scroller.scrollTop);

        return new Promise(function(resolve) {
          listenOnce(window, 'popstate', resolve);
          settings.navigateToPreviousRoute();
        });
      }).then(function() {
        // Should be at the top of the page after going Back from the section.
        assertEquals(0, page.scroller.scrollTop);

        return new Promise(function(resolve) {
          listenOnce(window, 'popstate', resolve);
          window.history.forward();
        });
      }).then(function() {
        // Should scroll when navigating forwards from the BASIC page.
        assertNotEquals(0, page.scroller.scrollTop);
      });
    });

    test('scroll to section after exiting search', function() {
      var searchManager = new TestSearchManager();
      settings.setSearchManagerForTesting(searchManager);

      settings.navigateTo(settings.Route.BASIC,
                          new URLSearchParams(`search=foobar`),
                          /* removeSearch */ false);
      return searchManager.whenCalled('search').then(function() {
        return new Promise(function(resolve) {
          settings.navigateTo(settings.Route.ON_STARTUP,
                              /* dynamicParams */ null,
                              /* removeSearch */ true);

          var page = self.getPage('basic');
          assertTrue(!!page);

          // Should (after some time) be scrolled to the On Startup section.
          var intervalId = window.setInterval(function() {
            if (page.scroller.scrollTop != 0) {
              window.clearInterval(intervalId);
              resolve();
            }
          }, 55);
        });
      });
    });

    test('scroll to top before navigating to about', function() {
      var page = self.getPage('basic');
      // Set the viewport small to force the scrollbar to appear on ABOUT.
      Polymer.dom().querySelector('settings-ui').style.height = '200px';

      settings.navigateTo(settings.Route.ON_STARTUP);
      assertNotEquals(0, page.scroller.scrollTop);

      settings.navigateTo(settings.Route.ABOUT);
      assertEquals(0, page.scroller.scrollTop);
    });
  });

  // Run all registered tests.
  mocha.run();
});
