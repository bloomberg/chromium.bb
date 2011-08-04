// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Tries to prerender two pages, one that will fail and one that will succeed.
 * Checks that we see all relevant events, and update the corresponding tables.
 * The prerender that will fail will briefly be active before it fails.  Having
 * an active prerender will block another prerender from starting too soon, so
 * |failureUrl| must be prerendered first.
 */
netInternalsTest.test('NetInternalsPrerenderView',
                      function (failureUrl, successUrl) {
  // IDs for special HTML elements in prerender_view.html
  var HISTORY_DIV_ID = 'prerender-view-history-div';
  var ACTIVE_DIV_ID = 'prerender-view-active-div';

  // Phases of the test.
  const STATE = {
    // We've switched to the prerender tab, but have yet to receive the
    // resulting onPrerenderInfoChanged event with no prerenders active or in
    // the history.
    START: 0,
    // We've added the prefetch link for |failureUrl|.  We may receive more
    // than one event while in this state, as we may see it as active once
    // or more before it moves to the history.  We will not receive any
    // event with both history and active prerenders empty in this state,
    // as we only send notifications when the values change.
    FAILURE_URL_LINKED: 1,
    // We've added the prefetch link for |successUrl|.
    SUCCESS_URL_LINKED: 2
  };

  /**
   * Observer responsible for running the test and checking results.
   * @param {string} failureUrl URL that can't be prerendered.
   * @param {string} successUrl URL that can be prerendered.
   * @constructor
   */
  function PrerenderTestObserver(failureUrl, successUrl) {
    // True if we've started prerendering |successUrl|.
    this.startedSuccessfulPrerender_ = false;
    this.failureUrl_ = failureUrl;
    this.successUrl_ = successUrl;
    this.state_ = STATE.START;
  }

  PrerenderTestObserver.prototype = {
    /**
     * Main function of the observer.  Tracks state transitions, checks the
     * table sizes, and does some sanity checking on received data.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    onPrerenderInfoChanged: function(prerenderInfo) {
      console.log('State: ' + this.state_);

      // Verify that prerendering is enabled.
      assertTrue(prerenderInfo.enabled, 'Prerendering not enabled.');

      // Check number of rows in both tables.
      netInternalsTest.checkStyledTableRows(HISTORY_DIV_ID,
                                            prerenderInfo.history.length);
      netInternalsTest.checkStyledTableRows(ACTIVE_DIV_ID,
                                            prerenderInfo.active.length);

      if (this.state_ == STATE.START) {
        this.start_(prerenderInfo);
      } else if (this.state_ == STATE.FAILURE_URL_LINKED) {
        this.failureUrlLinked_(prerenderInfo);
      } else if (this.state_ == STATE.SUCCESS_URL_LINKED) {
        this.successUrlLinked_(prerenderInfo);
      }
    },

    /**
     * Start by triggering a prerender of |failureUrl_|.
     * At this point, we expect no active or historical prerender entries.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    start_: function(prerenderInfo) {
      expectEquals(0, prerenderInfo.active.length);
      expectEquals(0, prerenderInfo.history.length);

      // Adding the url we expect to fail.
      addPrerenderLink(this.failureUrl_);
      this.state_ = STATE.FAILURE_URL_LINKED;
    },

    /**
     * We expect to either see the failure url as an active entry, or see it
     * move straight to the history.  In the latter case, we skip a state.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    failureUrlLinked_: function(prerenderInfo) {
      // May see the failure url as active, or may see it move straight to the
      // history.  If not, skip to the next state.
      if (prerenderInfo.active.length == 1) {
        expectEquals(this.failureUrl_, prerenderInfo.active[0].url);
        expectEquals(0, prerenderInfo.history.length);
        return;
      }

      // The prerender of |failureUrl_| has been cancelled, and is now in the
      // history.  Go ahead and prerender |successUrl_|.
      this.prerenderSuccessUrl_(prerenderInfo);
    },

    /**
     * Prerender |successUrl_|.  The prerender of |failureUrl_| should have
     * failed, and it should now be in the history.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    prerenderSuccessUrl_: function(prerenderInfo) {
      // We may see the duration of the active prerender increase.  If so,
      // do nothing.
      if (prerenderInfo.active.length == 1)
        return;

      assertEquals(1, prerenderInfo.history.length);
      expectEquals(this.failureUrl_, prerenderInfo.history[0].url);
      expectEquals(0, prerenderInfo.active.length);

      addPrerenderLink(this.successUrl_);
      this.state_ = STATE.SUCCESS_URL_LINKED;
    },

    /**
     * At this point, we expect to see the failure url in the history, and the
     * successUrl in the active entry list, and the test is done.
     * @param {Object} prerenderInfo State of prerendering pages.
     */
    successUrlLinked_: function(prerenderInfo) {
      assertEquals(1, prerenderInfo.history.length);
      expectEquals(this.failureUrl_, prerenderInfo.history[0].url);
      assertEquals(1, prerenderInfo.active.length);
      expectEquals(this.successUrl_, prerenderInfo.active[0].url);
      netInternalsTest.testDone();
    },
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
  var prerenderObserver = new PrerenderTestObserver(failureUrl, successUrl);
  g_browser.addPrerenderInfoObserver(prerenderObserver);
});
