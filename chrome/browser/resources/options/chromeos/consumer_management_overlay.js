// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var Page = cr.ui.pageManager.Page;
  var PageManager = cr.ui.pageManager.PageManager;

  /**
   * ConsumerManagementOverlay class
   * Dialog that allows users to enroll/unenroll consumer management service.
   * @constructor
   * @extends {cr.ui.pageManager.Page}
   */
  function ConsumerManagementOverlay() {
    Page.call(this, 'consumer-management-overlay',
              loadTimeData.getString('consumerManagementOverlayTabTitle'),
              'consumer-management-overlay');

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

  /**
   * Consumer management status.
   * See chrome/browser/chromeos/policy/consumer_management_service.h.
   * @enum {string}
   */
  ConsumerManagementOverlay.Status = {
    STATUS_UNKNOWN: 'StatusUnknown',
    STATUS_ENROLLED: 'StatusEnrolled',
    STATUS_ENROLLING: 'StatusEnrolling',
    STATUS_UNENROLLED: 'StatusUnenrolled',
    STATUS_UNENROLLING: 'StatusUnenrolling'
  };

  /**
   * Shows enrollment or unenrollment content based on the status.
   * @enum {string} status Consumer management service status string.
   */
  ConsumerManagementOverlay.setStatus = function(status) {
    // Status should only be enrolled or unenrolled.
    assert(status == this.Status.STATUS_ENROLLED ||
           status == this.Status.STATUS_UNENROLLED);
    var enrolled = status == this.Status.STATUS_ENROLLED;
    $('enroll-content').hidden = enrolled;
    $('unenroll-content').hidden = !enrolled;
  };

  // Export
  return {
    ConsumerManagementOverlay: ConsumerManagementOverlay
  };
});
