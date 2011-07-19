// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Implements the NavigationCollector object that powers the extension.
 *
 * @author mkwst@google.com (Mike West)
 */

/**
 * Collects navigation events, and provides a list of successful requests
 * that you can do interesting things with. Calling the constructor will
 * automatically bind handlers to the relevant webnavigation API events,
 * and to a `getMostRequestedUrls` extension message for internal
 * communication between background pages and popups.
 *
 * @constructor
 */
function NavigationCollector() {
  /**
   * A list of currently pending requests, implemented as a hash of each
   * request's tab ID, frame ID, and URL in order to ensure uniqueness.
   *
   * @type {Object.<string, {start: number}>}
   * @private
   */
  this.pending_ = {};

  /**
   * A list of completed requests, implemented as a hash of each
   * request's tab ID, frame ID, and URL in order to ensure uniqueness.
   *
   * @type {Object.<string, Array.<NavigationCollector.Request>>}
   * @private
   */
  this.completed_ = {};

  /**
   * A list of requests that errored off, implemented as a hash of each
   * request's tab ID, frame ID, and URL in order to ensure uniqueness.
   *
   * @type {Object.<string, Array.<NavigationCollector.Request>>}
   * @private
   */
  this.errored_ = {};

  // Bind handlers to the 'webNavigation' events that we're interested
  // in handling in order to build up a complete picture of the whole
  // navigation event.
  chrome.experimental.webNavigation.onBeforeRetarget.addListener(
      this.onBeforeRetargetListener_.bind(this));
  chrome.experimental.webNavigation.onBeforeNavigate.addListener(
      this.onBeforeNavigateListener_.bind(this));
  chrome.experimental.webNavigation.onCompleted.addListener(
      this.onCompletedListener_.bind(this));
  chrome.experimental.webNavigation.onCommitted.addListener(
      this.onCommittedListener_.bind(this));
  chrome.experimental.webNavigation.onErrorOccurred.addListener(
      this.onErrorOccurredListener_.bind(this));

  // Bind handler to extension messages for communication from popup.
  chrome.extension.onRequest.addListener(this.onRequestListener_.bind(this));
}

///////////////////////////////////////////////////////////////////////////////

/**
 * The possible transition types that explain how the navigation event
 * was generated (i.e. "The user clicked on a link." or "The user submitted
 * a form").
 *
 * @see http://code.google.com/chrome/extensions/trunk/history.html
 * @enum {string}
 */
NavigationCollector.NavigationType = {
  AUTO_BOOKMARK: 'auto_bookmark',
  AUTO_SUBFRAME: 'auto_subframe',
  FORM_SUBMIT: 'form_submit',
  GENERATED: 'generated',
  KEYWORD: 'keyword',
  KEYWORD_GENERATED: 'keyword_generated',
  LINK: 'link',
  MANUAL_SUBFRAME: 'manual_subframe',
  RELOAD: 'reload',
  START_PAGE: 'start_page',
  TYPED: 'typed'
};

/**
 * The possible transition qualifiers:
 *
 * * CLIENT_REDIRECT: Redirects caused by JavaScript, or a refresh meta tag
 *   on a page.
 *
 * * SERVER_REDIRECT: Redirected by the server via a 301/302 response.
 *
 * * FORWARD_BACK: User used the forward or back buttons to navigate through
 *   her browsing history.
 *
 * @enum {string}
 */
NavigationCollector.NavigationQualifier = {
  CLIENT_REDIRECT: 'client_redirect',
  FORWARD_BACK: 'forward_back',
  SERVER_REDIRECT: 'server_redirect'
};

/**
 * @typedef {{url: string, transitionType: NavigationCollector.NavigationType,
 *     transitionQualifier: Array.<NavigationCollector.NavigationQualifier>,
 *     openedInNewTab: boolean, sourceUrl: ?string, duration: number}}
 */
NavigationCollector.Request;

///////////////////////////////////////////////////////////////////////////////

NavigationCollector.prototype = {
  /**
   * Returns a somewhat unique ID for a given WebNavigation request.
   *
   * @param {!{tabId: number, frameId: number, url: string}} data Information
   *     about the navigation event we'd like an ID for.
   * @return {!string} ID created by combining the tab ID and frame ID (as the
   *     API ensures that these will be unique across a single navigation
   *     event)
   * @private
   */
  parseId_: function(data) {
    return data.tabId + '-' + data.frameId;
  },


  /**
   * Creates an empty entry in the pending array, and prepopulates the
   * errored and completed arrays for ease of insertion later.
   *
   * @param {!string} id The request's ID, as produced by parseId_.
   * @param {!string} url The request's URL.
   */
  prepareDataStorage_: function(id, url) {
    this.pending_[id] = this.pending_[id] || {
      openedInNewTab: false,
      sourceUrl: null,
      start: null,
      transitionQualifiers: [],
      transitionType: null
    };
    this.completed_[url] = this.completed_[url] || [];
    this.errored_[url] = this.errored_[url] || [];
  },


  /**
   * Handler for the 'onBeforeRetarget' event. Updates the pending request
   * with a sourceUrl, and notes that it was opened in a new tab.
   *
   * Pushes the request onto the
   * 'pending_' object, and stores it for later use.
   *
   * @param {!Object} data The event data generated for this request.
   * @private
   */
  onBeforeRetargetListener_: function(data) {
    var id = this.parseId_(data);
    this.prepareDataStorage_(id, data.url);
    this.pending_[id].openedInNewTab = true;
    this.pending_[id].sourceUrl = data.sourceUrl;
    this.pending_[id].start = data.timeStamp;
  },


  /**
   * Handler for the 'onBeforeNavigate' event. Pushes the request onto the
   * 'pending_' object, and stores it for later use.
   *
   * @param {!Object} data The event data generated for this request.
   * @private
   */
  onBeforeNavigateListener_: function(data) {
    var id = this.parseId_(data);
    this.prepareDataStorage_(id, data.url);
    this.pending_[id].start = this.pending_[id].start || data.timeStamp;
  },


  /**
   * Handler for the 'onCommitted' event. Updates the pending request with
   * transition information.
   *
   * Pushes the request onto the
   * 'pending_' object, and stores it for later use.
   *
   * @param {!Object} data The event data generated for this request.
   * @private
   */
  onCommittedListener_: function(data) {
    var id = this.parseId_(data);
    if (!this.pending_[id]) {
      console.warn(
          chrome.i18n.getMessage('errorCommittedWithoutPending'),
          data.url,
          data);
    } else {
      this.pending_[id].transitionType = data.transitionType;
      this.pending_[id].transitionQualifiers =
          data.transitionQualifiers;
    }
  },


  /**
   * Handler for the 'onCompleted` event. Pulls the request's data from the
   * 'pending_' object, combines it with the completed event's data, and pushes
   * a new NavigationCollector.Request object onto 'completed_'.
   *
   * @param {!Object} data The event data generated for this request.
   * @private
   */
  onCompletedListener_: function(data) {
    var id = this.parseId_(data);
    if (!this.pending_[id]) {
      console.warn(
          chrome.i18n.getMessage('errorCompletedWithoutPending'),
          data.url,
          data);
    } else {
      this.completed_[data.url].push({
        duration: (data.timeStamp - this.pending_[id].start),
        openedInNewWindow: this.pending_[id].openedInNewWindow,
        sourceUrl: this.pending_[id].sourceUrl,
        transitionQualifiers: this.pending_[id].transitionQualifiers,
        transitionType: this.pending_[id].transitionType,
        url: data.url
      });
      delete this.pending_[id];
    }
  },


  /**
   * Handler for the 'onErrorOccurred` event. Pulls the request's data from the
   * 'pending_' object, combines it with the completed event's data, and pushes
   * a new NavigationCollector.Request object onto 'errored_'.
   *
   * @param {!Object} data The event data generated for this request.
   * @private
   */
  onErrorOccurredListener_: function(data) {
    var id = this.parseId_(data);
    if (!this.pending_[id]) {
      console.error(
          chrome.i18n.getMessage('errorErrorOccurredWithoutPending'),
          data.url,
          data);
    } else {
      this.errored_[data.url].push({
        duration: (data.timeStamp - this.pending_[id].start),
        openedInNewWindow: this.pending_[id].openedInNewWindow,
        sourceUrl: this.pending_[id].sourceUrl,
        transitionQualifiers: this.pending_[id].transitionQualifiers,
        transitionType: this.pending_[id].transitionType,
        url: data.url
      });
      delete this.pending_[id];
    }
  },

  /**
   * Handle request messages from the popup.
   *
   * @param {!{type:string}} request The external request to answer.
   * @param {!MessageSender} sender Info about the script context that sent
   *     the request.
   * @param {!function} sendResponse Function to call to send a response.
   * @private
   */
  onRequestListener_: function(request, sender, sendResponse) {
    if (request.type === 'getMostRequestedUrls')
      sendResponse({result: this.getMostRequestedUrls(request.num)});
    else
      sendResponse({});
  },

///////////////////////////////////////////////////////////////////////////////

  /**
   * @return {Object.<string, NavigationCollector.Request>} The complete list of
   *     successful navigation requests.
   */
  get completed() {
    return this.completed_;
  },


  /**
   * @return {Object.<string, Navigationcollector.Request>} The complete list of
   *     unsuccessful navigation requests.
   */
  get errored() {
    return this.errored_;
  },


  /**
   * Get a list of the X most requested URLs.
   *
   * @param {number=} num The number of successful navigation requests to
   *     return. If 0 is passed in, or the argument left off entirely, all
   *     successful requests are returned.
   * @return {Object.<string, NavigationCollector.Request>} The list of
   *     successful navigation requests, sorted in decending order of frequency.
   */
  getMostRequestedUrls: function(num) {
    return this.getMostFrequentUrls_(this.completed, num);
  },


  /**
   * Get a list of the X most errored URLs.
   *
   * @param {number=} num The number of unsuccessful navigation requests to
   *     return. If 0 is passed in, or the argument left off entirely, all
   *     successful requests are returned.
   * @return {Object.<string, NavigationCollector.Request>} The list of
   *     unsuccessful navigation requests, sorted in decending order
   *     of frequency.
   */
  getMostErroredUrls: function(num) {
    return this.getMostErroredUrls_(this.errored, num);
  },


  /**
   * Get a list of the most frequent URLs in a list.
   *
   * @param {NavigationCollector.Request} list A list of URLs to parse.
   * @param {number=} num The number of navigation requests to return. If
   *     0 is passed in, or the argument left off entirely, all requests
   *     are returned.
   * @return {Object.<string, NavigationCollector.Request>} The list of
   *     navigation requests, sorted in decending order of frequency.
   * @private
   */
  getMostFrequentUrls_: function(list, num) {
    var result = [];
    var avg;
    // Convert the 'completed_' object to an array.
    for (var x in list) {
      avg = 0;
      if (list.hasOwnProperty(x)) {
        list[x].forEach(function(o) {
          avg += o.duration;
        });
        avg = avg / list[x].length;
        result.push({
          url: x,
          numRequests: list[x].length,
          requestList: list[x],
          average: avg
        });
      }
    }
    // Sort the array.
    result.sort(function(a, b) {
      return b.numRequests - a.numRequests;
    });
    // Return the requested number of results.
    return num ? result.slice(0, num) : result;
  }
};
