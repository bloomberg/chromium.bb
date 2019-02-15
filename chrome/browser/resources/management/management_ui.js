// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *    messages: !Array<string>,
 *    icon: string,
 * }}
 */
management.BrowserReportingData;

Polymer({
  is: 'management-ui',

  behaviors: [
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * List of messages related to browser reporting.
     * @private {?Array<!management.BrowserReportingData>}
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

    this.addWebUIListener(
        'browser-reporting-info-updated',
        reportingInfo => this.onBrowserReportingInfoReceived_(reportingInfo));

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
   * @param {!Array<!management.BrowserReportingResponse>} reportingInfo
   * @private
   */
  onBrowserReportingInfoReceived_(reportingInfo) {
    const reportingInfoMap = reportingInfo.reduce((info, response) => {
      info[response.reportingType] = info[response.reportingType] || {
        icon: this.getIconForReportingType_(response.reportingType),
        messages: []
      };
      info[response.reportingType].messages.push(
          loadTimeData.getString(response.messageId));
      return info;
    }, {});

    const reportingTypeOrder = {
      [management.ReportingType.SECURITY]: 1,
      [management.ReportingType.EXTENSIONS]: 2,
      [management.ReportingType.USER]: 3,
      [management.ReportingType.DEVICE]: 4,
    };

    this.browserReportingInfo_ =
        Object.keys(reportingInfoMap)
            .sort((a, b) => reportingTypeOrder[a] - reportingTypeOrder[b])
            .map(reportingType => reportingInfoMap[reportingType]);
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

  /**
   * @param {management.ReportingType} reportingType
   * @returns {string} The associated icon.
   * @private
   */
  getIconForReportingType_(reportingType) {
    switch (reportingType) {
      case management.ReportingType.SECURITY:
        return 'cr:security';
      case management.ReportingType.DEVICE:
        return 'cr:computer';
      case management.ReportingType.EXTENSIONS:
        return 'cr:extension';
      case management.ReportingType.USER:
        return 'cr:person';
      default:
        return 'cr:security';
    }
  },
});
