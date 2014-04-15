// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the tests for detecting extension's ad injection in
// Chrome. This contains many different, independent tests, but it is all run
// as a "single" browser test. The reason for this is that we want to do many
// short tests for ad injection, and the set-up/tear-down time for a browsertest
// implementation of each would be prohibitive.
// See also chrome/browser/extensions/activity_log/ad_injection_browsertest.cc
//
// For all you data lovers, a few quick benchmarks.
// Taken on a HP Z620 Goobuntu Workstation.
// Method 1: Running all tests within a single browsertest, and cleaning up on
//           the JS side after each (i.e., the method we're using).
// Method 2: Running so that every test has it's own browsertest.
//
// ----------------------------------------------------------
// Number of Tests | Method |   Real   |   User   |   Sys   |
//        1        |    1   |  6.002s  |  2.850s  |  0.580s |
//        10       |    1   |  8.387s  |  3.610s  |  0.920s |
//        100      |    1   | 33.765s  | 11.740s  |  3.000s |
//        1        |    2   |  6.020s  |  2.940s  |  0.470s |
//        10       |    2   | 57.945s  | 26.580s  |  4.950s |
//        100      |    2   |   N/A    |   N/A    |   N/A   |
// ----------------------------------------------------------

// This html block is just designed to be a massive playground for ad injectors,
// where they can let loose and have fun.
// We use this to populate our page at the start of every test.
var kBodyHtml =
    '<iframe id="ad-iframe" src="http://www.known-ads.adnetwork"></iframe>' +
    '<iframe id="non-ad-iframe" src="http://www.not-ads.adnetwork"></iframe>' +
    '<embed id="ad-embed" src="http://www.known-ads.adnetwork"><embed>' +
    '<embed id="non-ad-embed" src="http://www.not-ads.adnetwork"><embed>' +
    '<a id="ad-link" href="http://www.known-ads.adnetwork"></a>' +
    '<a id="non-ad-link" href="http://www.not-ads.adnetwork"></a>' +
    '<div id="empty-div"></div>';



/**
 * The AdInjectorTest infrastructure. Basically, this allows the test to follow
 * a simple iteration cycle:
 * - Signal the C++ test that we're going to do page setup, and we shouldn't
 *   record any ad-injector events (as some of the page setup could qualify).
 * - Do the page setup, which involves creating a 'playground' for ad injectors
 *   of iframes, embeds, etc. See also ad_injectors.html. We do this set up for
 *   each test, so that none of the tests conflicts with each other.
 * - Signal that we are done with page setup, and should start recording events.
 * - Run the next test.
 * - Signal that the test is done, and we should check for the result.
 *
 * This cycle repeats, and should be done synchronously so that we don't end up
 * recording events which we shouldn't, and can attribute each event to its
 * cause.
 * @constructor
 */
function AdInjectorTest(functions) {
  /*
   * The list of functions to run in order to test ad injection.
   * @type {Array.<Function>}
   * @private
   */
  this.functions_ = functions;
};

AdInjectorTest.prototype = {
  /**
   * The index of the function that is next in line.
   * @type {int}
   * @private
   */
  currentFunction_: 0,

  /**
   * Start the page reset process. This includes sending a message to Chrome
   * and verifying receipt (by waiting for a response, and then triggering the
   * reset.
   * @private
   */
  resetPageAndRunNextFunction_: function() {
    chrome.test.sendMessage('Page Reset Begin', function(response) {
      this.resetPageAndRunNextFunctionImpl_();
    }.bind(this));
  },

  /**
   * Reset the page. This means eliminating any previous "content" from the
   * body, and refreshing it with a new, clean slate.
   * @private
   */
  resetPageAndRunNextFunctionImpl_: function() {
    document.body.innerHTML = kBodyHtml;
    chrome.test.sendMessage('Page Reset End', function(response) {
      this.runNextFunctionImpl_();
    }.bind(this));
  },

  /**
   * Start the process to run the next function (which begins by resetting the
   * page). If there is no next function, send the "Test Complete" message.
   */
  runNextFunction: function() {
    if (this.currentFunction_ < this.functions_.length)
      this.resetPageAndRunNextFunction_();
    else
      chrome.test.sendMessage('Test Complete');
  },

  /**
   * The implementation of runNextFunction().
   * @private
   */
  runNextFunctionImpl_: function() {
    var f = this.functions_[this.currentFunction_++];
    var message = '';

    // Try and catch any errors so one bad function does cause the whole suite
    // to crash.
    try {
      var change = f();
      message = f.name + ':' + change;
    } catch(e) {
      message = 'Testing Error:' + f.name + ':' + e.message + ', ' + e.stack;
    }

    chrome.test.sendMessage(message, function(response) {
      this.runNextFunction();
    }.bind(this));
  }
};

// Return values to signal the result of a test. Each test should return the
// appropriate value in order to indicate if an ad was injected, and, if so,
// what kind of injection.
// These match the enum values in
// chrome/browser/extensions/activity_log/activity_actions.h.
var NO_AD_INJECTION = 0;        // The signal that there was no ad injection.
var INJECTION_NEW_AD = 1;       // The signal that there was a new ad injected.
var INJECTION_REMOVED_AD = 2;   // The signal that an ad was removed.
var INJECTION_REPLACED_AD = 3;  // The signal that an ad was replaced.

/* The "ad network" url to use for tests. */
var kAdNetwork = 'http://www.known-ads.adnetwork';

/* The "non ad network" url to use for tests. */
var kNonAdNetwork = 'http://www.not-ads.adnetwork';

/* @return {?HTMLElement} The element with the given id. */
var $ = function(id) { return document.getElementById(id); }

// The following is a collection of functions to "get" an HTML ad. We need to do
// this for internal counting in testing, because if we simply do:
//   var iframe = document.createElement('iframe');
//   iframe.src = kAdNetwork;
//   document.body.appendChild(iframe);
// We end up double counting ad injections. We count one for modifying the src
// of the iframe (on line 2) and a second for appending an iframe with an ad
// (on line 3). Instead of doing this, use the get*Ad() functions below. The
// example above would become:
//   document.body.appendChild(getIframeAd());

// Creates an iframe ad, like so:
// <iframe src="http://www.known-ads.adnetwork"></iframe>
var kIframeAdTemplate = document.createElement('iframe');
kIframeAdTemplate.src = kAdNetwork;

/**
 * @return An iframe element which will count as ad injection in the tests.
 */
var getIframeAd = function() {
  return kIframeAdTemplate.cloneNode(true);
};

// Creates an embed ad, like so:
// <embed src="http://www.known-ads.adnetwork"></embed>
var kEmbedAdTemplate = document.createElement('embed');
kEmbedAdTemplate.src = kAdNetwork;

/**
 * @return An embed element which will count as ad injection in the tests.
 */
var getEmbedAd = function() {
  return kEmbedAdTemplate.cloneNode(true);
};

// Creates a link ad, like so:
// <a href="http://www.known-ads.adnetwork"></a>
var kLinkAdTemplate = document.createElement('a');
kLinkAdTemplate.href = kAdNetwork;

/**
 * @return A link ('a') element which will count as ad injection in the tests.
 */
var getLinkAd = function() {
  return kLinkAdTemplate.cloneNode(true);
};

// This series constructs a nested ad, which looks like this:
// <div>
//   <div>
//     <span></span>
//     <iframe src="http://www.known-ads.adnetwork"></iframe>
//   </div>
// </div>
var div = document.createElement('div');
div.appendChild(document.createElement('span'));
div.appendChild(getIframeAd());
var kNestedAdTemplate = document.createElement('div');
kNestedAdTemplate.appendChild(div);

/**
 * @return A div with a nested element which will count as ad injection in the
 *     tests.
 */
var getNestedAd = function() {
  return kNestedAdTemplate.cloneNode(true);
};

/*
 * The collection of functions to use for testing.
 * In order to add a new test, simply append it to the collection of functions.
 * All functions will be run in the test, and each will report its success or
 * failure independently of the others.
 * All test functions must be synchronous.
 * @type {Array.<Function>}
 */
var functions = [];

// Add a new iframe with an AdNetwork source by creating the element and
// appending it.
functions.push(function NewIframeAdNetwork() {
  document.body.appendChild(getIframeAd());
  return INJECTION_NEW_AD;
});

// Add a new iframe which does not serve ads. We should not record anything.
functions.push(function NewIframeNonAdNetwork() {
  var frame = document.createElement('iframe');
  frame.src = kNonAdNetwork;
  document.body.appendChild(frame);
  return NO_AD_INJECTION;
});

// Modify an iframe which is currently in the DOM, switching the src to an
// ad network.
functions.push(function ModifyExistingIframeToAdNetwork() {
  var frame = $('ad-iframe');
  frame.src = kAdNetwork;
  return INJECTION_NEW_AD;
});

// Add a new embed element which serves ads.
functions.push(function NewEmbedAdNetwork() {
  document.body.appendChild(getEmbedAd());
  return INJECTION_NEW_AD;
});

// Add a new embed element which does not serve ads. We should not record
// anything.
functions.push(function NewEmbedNonAdNetwork() {
  var embed = document.createElement('embed');
  embed.src = kNonAdNetwork;
  document.body.appendChild(embed);
  return NO_AD_INJECTION;
});

// Modify an embed which is currently in the DOM, switching the src to an
// ad network.
functions.push(function ModifyExistingEmbedToAdNetwork() {
  var embed = $('ad-embed');
  embed.src = kAdNetwork;
  return INJECTION_NEW_AD;
});

// Add a new link element which serves ads.
functions.push(function NewLinkAd() {
  document.body.appendChild(getLinkAd());
  return INJECTION_NEW_AD;
});

// Add a new element which has a nested ad, to ensure we do a deep check of
// elements appended to the dom.
functions.push(function NewNestedAd() {
  document.body.appendChild(getNestedAd());
  return INJECTION_NEW_AD;
});

// Verify that we do not enter the javascript world when we check for ad
// injection.
functions.push(function VerifyNoAccess() {
  var frame = document.createElement('iframe');
  frame.__defineGetter__('src', function() {
    throw new Error('Forbidden access into javascript execution!');
    return kAdNetwork;
  });
  return NO_AD_INJECTION;
});

// TODO(rdevlin.cronin): We are not covering every case yet. Fix this.
// See crbug.com/357204.

// Kick off the tests.
var test = new AdInjectorTest(functions);
test.runNextFunction();
