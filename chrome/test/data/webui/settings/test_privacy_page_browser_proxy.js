// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {TestBrowserProxy}
 * @implements {settings.PrivacyPageBrowserProxy}
 */
function TestPrivacyPageBrowserProxy() {
  settings.TestBrowserProxy.call(this, [
    'getMetricsReporting',
    'getSafeBrowsingExtendedReporting',
    'setMetricsReportingEnabled',
    'setSafeBrowsingExtendedReportingEnabled',
    'showManageSSLCertificates',
  ]);
}

TestPrivacyPageBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @type {!MetricsReporting} */
  metricsReporting: {
    enabled: true,
    managed: true,
  },

  /** @override */
  getMetricsReporting: function() {
    this.methodCalled('getMetricsReporting');
    return Promise.resolve(this.metricsReporting);
  },

  /** @override */
  setMetricsReportingEnabled: function(enabled) {
    this.methodCalled('setMetricsReportingEnabled', enabled);
  },

  /** @override */
  showManageSSLCertificates: function() {
    this.methodCalled('showManageSSLCertificates');
  },

  /** @override */
  getSafeBrowsingExtendedReporting: function() {
    this.methodCalled('getSafeBrowsingExtendedReporting');
    return Promise.resolve(true);
  },

  /** @override */
  setSafeBrowsingExtendedReportingEnabled: function(enabled) {
    this.methodCalled('setSafeBrowsingExtendedReportingEnabled', enabled);
  },
};
