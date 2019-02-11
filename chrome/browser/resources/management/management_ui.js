// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'management-ui',

  properties: {
    /**
     * List of messages related to browser reporting.
     * @private {?Array<string>}
     */
    browserReportingInfo_: Array,

    /**
     * List of messages related to browser reporting.
     * @private {?Array<!management.Extension>}
     */
    extensions_: Array,

    // <if expr="chromeos">
    /**
     * Message stating if the Trust Roots are configured.
     * @private
     */
    localTrustRoots_: String,
    // </if>

    /**
     * Indicates if the search field in visible in the toolbar.
     * @private
     */
    showSearchInToolbar_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?management.ManagementBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached() {
    document.documentElement.classList.remove('loading');
    this.browserProxy_ = management.ManagementBrowserProxyImpl.getInstance();
    this.initBrowserReportingInfo_();
    this.getExtensions_();
    // <if expr="chromeos">
    this.getLocalTrustRootsInfo_();
    // </if>
  },

  /** @private */
  initBrowserReportingInfo_() {
    this.browserProxy_.initBrowserReportingInfo().then(
        reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));
  },

  /**
   * @param {!Array<string>} reportingInfo
   * @private
   */
  onBrowserReportingInfoReceived_(reportingInfo) {
    this.browserReportingInfo_ =
        reportingInfo.map(id => loadTimeData.getString(id));
  },

  /** @private */
  getExtensions_() {
    this.browserProxy_.getExtensions().then(extensions => {
      this.extensions_ = extensions;
    });
  },

  // <if expr="chromeos">
  /** @private */
  getLocalTrustRootsInfo_() {
    this.browserProxy_.getLocalTrustRootsInfo().then(trustRootsConfigured => {
      this.localTrustRoots_ = loadTimeData.getString(
          trustRootsConfigured ? 'managementTrustRootsConfigured' :
                                 'managementTrustRootsNotConfigured');
    });
  },
  // </if>

  /**
   * @return {boolean} True of there are browser reporting info to show.
   * @private
   */
  showBrowserReportingInfo_() {
    return !!this.browserReportingInfo_ &&
        this.browserReportingInfo_.length > 0;
  },

  /**
   * @return {boolean} True of there are extension reporting info to show.
   * @private
   */
  showExtensionReportingInfo_() {
    return !!this.extensions_ && this.extensions_.length > 0;
  },
});
