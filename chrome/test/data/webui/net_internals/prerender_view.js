// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tries to prerender a page.  |shouldSucceed| indicates whether the prerender
 * is expected to succeed or not.  If it's false, we just wait for the page
 * to fail, possibly seeing it as active first.  If it's true, we open the
 * URL in another tab.  This is done via a message to the test handler, rather
 * than via Javascript, so we don't cancel the prerender since prerendering
 * can't currently set window.opener properly.
 *
 * Checks that we see all relevant events, and update the corresponding tables.
 * In both cases, we exit the test once we see the prerender in the history.
 * |finalStatus| is the expected status value when the page reaches the
 * history.
 */
netInternalsTest.test('netInternalsPrerenderView',
                      function (url, shouldSucceed, finalStatus) {
  // Phases of the test.
  const STATE = {
    // We've switched to the prerender tab, but have yet to receive the
    // resulting onPrerenderInfoChanged event with no prerenders active or in
    // the history.
    START: 0,
    // We've added the prerender link, but still need to open the link in a new
    // tab.  Only visit this state if |shouldSucceed| is true.
    NEED_OPEN_IN_NEW_TAB: 1,
    // We've added the prefetch link for |url|, opened a new tab if needed,
    // and are waiting for it to move to the history.  We may see the prerender
    // one or more times in the active list, or it may move straight to the
    // history.  We will not receive any event with both history and active
    // prerenders empty while in this state, as we only send notifications
    // when the values change.
    HISTORY_WAIT: 2
  };

  /**
   * Observer responsible for running the test and checking results.
   * @param {string} url URL to be prerendered.
   * @param {string} shouldSucceed Whether or not the prerender should succeed.
   * @param {string} finalStatus The expected value of |final_status|.
   * @constructor
   */
  function PrerenderTestObserver(url, shouldSucceed, finalStatus) {
    // True if we've started prerendering |successUrl|.
    this.startedSuccessfulPrerender_ = false;
    this.url_ = url;
    this.shouldSucceed_ = shouldSucceed;
    this.finalStatus_ = finalStatus;
    this.state_ = STATE.START;
  }

  PrerenderTestObserver.prototype = {
    /**
     * Main function of the observer.  Tracks state transitions, checks the
     * table sizes, and does some sanity checking on received data.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    onPrerenderInfoChanged: function(prerenderInfo) {
      // Verify that prerendering is enabled.
      assertTrue(prerenderInfo.enabled, 'Prerendering not enabled.');

      // Check number of rows in both tables.
      netInternalsTest.checkStyledTableRows(PrerenderView.HISTORY_DIV_ID,
                                            prerenderInfo.history.length);
      netInternalsTest.checkStyledTableRows(PrerenderView.ACTIVE_DIV_ID,
                                            prerenderInfo.active.length);

      if (this.state_ == STATE.START) {
        this.start_(prerenderInfo);
      } else if (this.state_ == STATE.NEED_OPEN_IN_NEW_TAB) {
        this.openInNewTab_(prerenderInfo);
      } else if (this.state_ == STATE.HISTORY_WAIT) {
        this.checkDone_(prerenderInfo);
      }
    },

    /**
     * Start by triggering a prerender of |url_|.
     * At this point, we expect no active or historical prerender entries.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    start_: function(prerenderInfo) {
      expectEquals(0, prerenderInfo.active.length);
      expectEquals(0, prerenderInfo.history.length);

      // Adding the url we expect to fail.
      addPrerenderLink(this.url_);
      if (this.shouldSucceed_) {
        this.state_ = STATE.NEED_OPEN_IN_NEW_TAB;
      } else {
        this.state_ = STATE.HISTORY_WAIT;
      }
    },

    /**
     * Starts opening |url_| in a new tab.
     * At this point, we expect the prerender to be active.
     * Only called if |shouldSucceed_| is true, and |urlOpenedInNewTab_| is
     * false.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    openInNewTab_: function(prerenderInfo) {
      expectEquals(0, prerenderInfo.history.length);
      assertEquals(1, prerenderInfo.active.length);
      expectEquals(this.url_, prerenderInfo.active[0].url);
      expectTrue(this.shouldSucceed_);

      chrome.send('openNewTab', [this.url_]);
      this.state_ = STATE.HISTORY_WAIT;
    },

    /**
     * We expect to either see the failure url as an active entry, or see it
     * in the history.  In the latter case, the test completes.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    checkDone_: function(prerenderInfo) {
      // If we see the url as active, continue running the test.
      if (prerenderInfo.active.length == 1) {
        expectEquals(this.url_, prerenderInfo.active[0].url);
        expectEquals(0, prerenderInfo.history.length);
        return;
      }

      // The prerender of |url_| is now in the history.
      this.checkHistory_(prerenderInfo);
    },

    /**
     * Check if the history is consistent with expectations, and end the test.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    checkHistory_: function(prerenderInfo) {
      expectEquals(0, prerenderInfo.active.length);
      assertEquals(1, prerenderInfo.history.length);
      expectEquals(this.url_, prerenderInfo.history[0].url);
      expectEquals(this.finalStatus_, prerenderInfo.history[0].final_status);

      testDone();
    }
  };

  /**
   * Adds a <link rel="prerender" href="url"> to the document.
   * @param {string} url URL of the page to prerender.
   */
  function addPrerenderLink(url) {
    var link = document.createElement('link');
    link.setAttribute('rel', 'prerender');
    link.setAttribute('href', url);
    document.body.appendChild(link);
  }

  netInternalsTest.switchToView('prerender');

  // Create the test observer, which will start the test once we see the initial
  // onPrerenderInfoChanged event from changing the active tab.
  var prerenderObserver = new PrerenderTestObserver(url, shouldSucceed,
                                                    finalStatus);
  g_browser.addPrerenderInfoObserver(prerenderObserver);
});
