// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('reset_page', function() {
  /**
   * @constructor
   * @implements {settings.ResetBrowserProxy}
   * @extends {settings.TestBrowserProxy}
   */
  var TestResetBrowserProxy = function() {
    settings.TestBrowserProxy.call(this, [
      'performResetProfileSettings',
      'onHideResetProfileDialog',
      'onHideResetProfileBanner',
      'onShowResetProfileDialog',
      'showReportedSettings',
      'getTriggeredResetToolName',
      'onPowerwashDialogShow',
    ]);
  };

  TestResetBrowserProxy.prototype = {
    __proto__: settings.TestBrowserProxy.prototype,

    /** @override */
    performResetProfileSettings: function(sendSettings, requestOrigin) {
      this.methodCalled('performResetProfileSettings', requestOrigin);
      return Promise.resolve();
    },

    /** @override */
    onHideResetProfileDialog: function() {
      this.methodCalled('onHideResetProfileDialog');
    },

    /** @override */
    onHideResetProfileBanner: function() {
      this.methodCalled('onHideResetProfileBanner');
    },

    /** @override */
    onShowResetProfileDialog: function() {
      this.methodCalled('onShowResetProfileDialog');
    },

    /** @override */
    showReportedSettings: function() {
      this.methodCalled('showReportedSettings');
    },

    /** @override */
    getTriggeredResetToolName: function() {
      this.methodCalled('getTriggeredResetToolName');
      return Promise.resolve('WonderfulAV');
    },

    /** @override */
    onPowerwashDialogShow: function() {
      this.methodCalled('onPowerwashDialogShow');
    },
  };

  return {
    TestResetBrowserProxy: TestResetBrowserProxy,
  };
});
