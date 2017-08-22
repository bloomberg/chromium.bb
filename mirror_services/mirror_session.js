// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface to a mirroring session.

 */

goog.provide('mr.mirror.Session');

goog.require('mr.TabUtils');


/**
 * Creates a new MirrorSession.  Do not call, as this is an abstract base class.
 */
mr.mirror.Session = class {
  /**
   * @param {!mr.Route} route
   */
  constructor(route) {
    /**
     * The media route associated with this mirror session.
     * @protected @const {!mr.Route}
     */
    this.route = route;

    /**
     * Whether the tab being mirrored is in a incognito window.
     * Valid for tab mirroring.
     * @protected {boolean}
     */
    this.isIncognito = false;

    /**
     * The URL of the icon associated with the content being mirrored.  This may
     * be sent to the mirroring receiver for non-incognito routes and tabs.
     *
     * @protected {?string}
     */
    this.iconUrl = null;

    /**
     * The title for local display.
     * For tab mirroring, it is tab title.
     * @protected {?string}
     */
    this.localTitle = null;

    /**
     * The title for sink media metadata.  This may be sent to the mirroring
     * receiver for non-incognito routes and tabs.  For tab mirroring, it is
     * '<tab
     * domain> (Tab)'.
     *
     * @protected {?string}
     */
    this.remoteTitle = null;

    /** @type {?number} */
    this.tabId = null;
  }

  /**
   * @param {string} localTitle The title for local display.
   * @param {string} remoteTitle The title for sink media metadata, which is
   *    given to other senders.
   * @param {?string} iconUrl The URL of the icon associated with the content
   *    being mirrored.
   */
  setMetadata(localTitle, remoteTitle, iconUrl) {
    this.route.description = this.localTitle = localTitle;
    if (!this.isIncognito && !this.route.offTheRecord) {
      this.remoteTitle = remoteTitle;
      this.iconUrl = iconUrl;
    } else {
      this.remoteTitle = null;
      this.iconUrl = null;
    }
    // Route description may have been updated.
    this.onRouteUpdated();
  }

  /**
   * Sends metadata about the content being mirrored to the sink,
   * if the content is not from an incognito tab or profile.
   */
  sendMetadataToSink() {
    if (!this.isIncognito && !this.route.offTheRecord)
      this.sendMetadataToSinkInternal();
  }

  /**
   * @param {number} tabId The id of the tab being captured.
   * @return {!Promise} Resolved after the session is updated with tab
   *     info.
   */
  updateTabInfoWithTabId(tabId) {
    this.tabId = tabId;
    return mr.TabUtils.getTab(tabId).then(this.updateTabInfo.bind(this));
  }

  /**
   * @param {!Tab} tab The tab being captured.
   */
  updateTabInfo(tab) {
    this.isIncognito = tab.incognito;
    const url = new URL(tab.url);
    this.setMetadata(
        tab.title,
        url.protocol == 'file:' ? 'Local content (Tab)' :
                                  url.hostname + ' (Tab)',
        tab.favIconUrl);
  }

  /**
   * @return {?string}
   */
  getLocalTitle() {
    return this.localTitle;
  }

  /**
   * @return {?string}
   */
  getIconUrl() {
    return this.iconUrl;
  }

  /**
   * @return {!mr.Route}
   */
  getRoute() {
    return this.route;
  }

  /**
   * Starts the mirroring session. The |mediaStream| must provide one audio
   * and/or one video track. It is illegal to call start() more than once on the
   * same session, even after stop() has been called. See updateStream(), or
   * else create a new instance to re-start a session.
   * @param {!MediaStream} mediaStream The media stream that has the audio
   *    and/or video track.
   * @return {!Promise<mr.mirror.Session>} Fulfilled when the
   *     session has been created and transports have started for the streams.
   */
  start(mediaStream) {}

  /**
   * @return {boolean} Whether the session supports updating its media stream
   *     while it is active.
   */
  supportsUpdateStream() {
    return false;
  }

  /**
   * Updates the media stream that the session is using.
   * @param {!MediaStream} mediaStream
   * @return {!Promise}
   */
  updateStream(mediaStream) {}

  /**
   * Stops the mirroring session. The underlying streams and transports are
   * stopped and destroyed. Sessions cannot be re-started. Instead, either use
   * updateStream() or create a new instance to re-start a session.
   * @return {!Promise<void>} Fulfilled with once the session has been stopped.
   *     The promise is rejected on error.
   */
  stop() {
    return Promise.resolve();
  }

  /**
   * Sends meta data to sink.
   * For example, when tab navigates, meta data info needs to be updated.
   */
  sendMetadataToSinkInternal() {}

  /**
   * Notifies the session that the associated route has been updated.
   */
  onRouteUpdated() {}
};
