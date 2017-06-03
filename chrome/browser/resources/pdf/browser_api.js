// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Returns a promise that will resolve to the default zoom factor.
 * @param {!Object} streamInfo The stream object pointing to the data contained
 *     in the PDF.
 * @return {Promise<number>} A promise that will resolve to the default zoom
 *     factor.
 */
function lookupDefaultZoom(streamInfo) {
  // Webviews don't run in tabs so |streamInfo.tabId| is -1 when running within
  // a webview.
  if (!chrome.tabs || streamInfo.tabId < 0)
    return Promise.resolve(1);

  return new Promise(function(resolve, reject) {
    chrome.tabs.getZoomSettings(streamInfo.tabId, function(zoomSettings) {
      resolve(zoomSettings.defaultZoomFactor);
    });
  });
}

/**
 * Returns a promise that will resolve to the initial zoom factor
 * upon starting the plugin. This may differ from the default zoom
 * if, for example, the page is zoomed before the plugin is run.
 * @param {!Object} streamInfo The stream object pointing to the data contained
 *     in the PDF.
 * @return {Promise<number>} A promise that will resolve to the initial zoom
 *     factor.
 */
function lookupInitialZoom(streamInfo) {
  // Webviews don't run in tabs so |streamInfo.tabId| is -1 when running within
  // a webview.
  if (!chrome.tabs || streamInfo.tabId < 0)
    return Promise.resolve(1);

  return new Promise(function(resolve, reject) {
    chrome.tabs.getZoom(streamInfo.tabId, resolve);
  });
}

/**
 * A class providing an interface to the browser.
 */
class BrowserApi {
  /**
   * @constructor
   * @param {!Object} streamInfo The stream object which points to the data
   *     contained in the PDF.
   * @param {number} defaultZoom The default browser zoom.
   * @param {number} initialZoom The initial browser zoom
   *     upon starting the plugin.
   * @param {BrowserApi.ZoomBehavior} zoomBehavior How to manage zoom.
   */
  constructor(streamInfo, defaultZoom, initialZoom, zoomBehavior) {
    this.streamInfo_ = streamInfo;
    this.defaultZoom_ = defaultZoom;
    this.initialZoom_ = initialZoom;
    this.zoomBehavior_ = zoomBehavior;
  }

  /**
   * Returns a promise to a BrowserApi.
   * @param {!Object} streamInfo The stream object pointing to the data
   *     contained in the PDF.
   * @param {BrowserApi.ZoomBehavior} zoomBehavior How to manage zoom.
   */
  static create(streamInfo, zoomBehavior) {
    return Promise.all([
        lookupDefaultZoom(streamInfo),
        lookupInitialZoom(streamInfo)
    ]).then(function(zoomFactors) {
      return new BrowserApi(
          streamInfo, zoomFactors[0], zoomFactors[1], zoomBehavior);
    });
  }

  /**
   * Returns the stream info pointing to the data contained in the PDF.
   * @return {Object} The stream info object.
   */
  getStreamInfo() {
    return this.streamInfo_;
  }

  /**
   * Aborts the stream.
   */
  abortStream() {
    if (chrome.mimeHandlerPrivate)
      chrome.mimeHandlerPrivate.abortStream();
  }

  /**
   * Sets the browser zoom.
   * @param {number} zoom The zoom factor to send to the browser.
   * @return {Promise} A promise that will be resolved when the browser zoom
   *     has been updated.
   */
  setZoom(zoom) {
    if (this.zoomBehavior_ != BrowserApi.ZoomBehavior.MANAGE)
      return Promise.reject(new Error('Viewer does not manage browser zoom.'));
    return new Promise(function(resolve, reject) {
      chrome.tabs.setZoom(this.streamInfo_.tabId, zoom, resolve);
    }.bind(this));
  }

  /**
   * Returns the default browser zoom factor.
   * @return {number} The default browser zoom factor.
   */
  getDefaultZoom() {
    return this.defaultZoom_;
  }

  /**
   * Returns the initial browser zoom factor.
   * @return {number} The initial browser zoom factor.
   */
  getInitialZoom() {
    return this.initialZoom_;
  }

  /**
   * Returns how to manage the zoom.
   * @return {BrowserApi.ZoomBehavior} How to manage zoom.
   */
  getZoomBehavior() {
    return this.zoomBehavior_;
  }

  /**
   * Adds an event listener to be notified when the browser zoom changes.
   * @param {!Function} listener The listener to be called with the new zoom
   *     factor.
   */
  addZoomEventListener(listener) {
    if (!(this.zoomBehavior_ == BrowserApi.ZoomBehavior.MANAGE ||
          this.zoomBehavior_ == BrowserApi.ZoomBehavior.PROPAGATE_PARENT))
      return;

    chrome.tabs.onZoomChange.addListener(function(info) {
      var zoomChangeInfo =
          /** @type {{tabId: number, newZoomFactor: number}} */ (info);
      if (zoomChangeInfo.tabId != this.streamInfo_.tabId)
        return;
      listener(zoomChangeInfo.newZoomFactor);
    }.bind(this));
  }
}

/**
 * Enumeration of ways to manage zoom changes.
 * @enum {number}
 */
BrowserApi.ZoomBehavior = {
  NONE: 0,
  MANAGE: 1,
  PROPAGATE_PARENT: 2
};

/**
 * Creates a BrowserApi for an extension running as a mime handler.
 * @return {Promise<BrowserApi>} A promise to a BrowserApi instance constructed
 *     using the mimeHandlerPrivate API.
 */
function createBrowserApiForMimeHandlerView() {
  return new Promise(function(resolve, reject) {
    chrome.mimeHandlerPrivate.getStreamInfo(resolve);
  }).then(function(streamInfo) {
    let promises = [];
    let zoomBehavior = BrowserApi.ZoomBehavior.NONE;
    if (streamInfo.tabId != -1) {
      zoomBehavior = streamInfo.embedded ?
                      BrowserApi.ZoomBehavior.PROPAGATE_PARENT :
                      BrowserApi.ZoomBehavior.MANAGE;
      promises.push(new Promise(function(resolve) {
        chrome.tabs.get(streamInfo.tabId, resolve);
      }).then(function(tab) {
        if (tab)
          streamInfo.tabUrl = tab.url;
      }));
    }
    if (zoomBehavior == BrowserApi.ZoomBehavior.MANAGE) {
      promises.push(new Promise(function(resolve) {
        chrome.tabs.setZoomSettings(
            streamInfo.tabId, {mode: 'manual', scope: 'per-tab'}, resolve);
      }));
    }
    return Promise.all(promises).then(
        function() { return BrowserApi.create(streamInfo, zoomBehavior); });
  });
}

/**
 * Creates a BrowserApi instance for an extension not running as a mime handler.
 * @return {Promise<BrowserApi>} A promise to a BrowserApi instance constructed
 *     from the URL.
 */
function createBrowserApiForPrintPreview() {
  let url = window.location.search.substring(1);
  let streamInfo = {
    streamUrl: url,
    originalUrl: url,
    responseHeaders: {},
    embedded: window.parent != window,
    tabId: -1,
  };
  return new Promise(function(resolve, reject) {
    if (!chrome.tabs) {
      resolve();
      return;
    }
    chrome.tabs.getCurrent(function(tab) {
      streamInfo.tabId = tab.id;
      streamInfo.tabUrl = tab.url;
      resolve();
    });
  }).then(function() {
    return BrowserApi.create(streamInfo, BrowserApi.ZoomBehavior.NONE);
  });
}

/**
 * Returns a promise that will resolve to a BrowserApi instance.
 * @return {Promise<BrowserApi>} A promise to a BrowserApi instance for the
 *     current environment.
 */
function createBrowserApi() {
  if (location.origin === 'chrome://print') {
    return createBrowserApiForPrintPreview();
  }

  return createBrowserApiForMimeHandlerView();
}
