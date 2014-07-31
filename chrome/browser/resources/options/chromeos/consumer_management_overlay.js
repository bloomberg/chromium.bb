// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * ConsumerManagementOverlay class
   * Dialog that allows users to enroll/unenroll consumer management service.
   * @extends {SettingsDialog}
   * @constructor
   */
  function ConsumerManagementOverlay() {
    Page.call(this, 'consumer-management-overlay',
              loadTimeData.getString('consumerManagementOverlayTabTitle'),
              'consumer-management-overlay');

    var isEnrolled = loadTimeData.getBoolean('consumerManagementEnrolled');
    $('enroll-content').hidden = isEnrolled;
    $('unenroll-content').hidden = !isEnrolled;

    $('consumer-management-overlay-enroll').onclick = function(event) {
      chrome.send('enrollConsumerManagement');
      PageManager.closeOverlay();
    };
    $('consumer-management-overlay-unenroll').onclick = function(event) {
      chrome.send('unenrollConsumerManagement');
      PageManager.closeOverlay();
    };
    $('consumer-management-overlay-enroll-cancel').onclick = function(event) {
      PageManager.closeOverlay();
    };
    $('consumer-management-overlay-unenroll-cancel').onclick = function(event) {
      PageManager.closeOverlay();
    };
  }

  cr.addSingletonGetter(ConsumerManagementOverlay);

  ConsumerManagementOverlay.prototype = {
    __proto__: Page.prototype,
  };

  // Export
  return {
    ConsumerManagementOverlay: ConsumerManagementOverlay
  };
});
