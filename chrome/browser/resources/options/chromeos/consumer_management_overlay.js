// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  var OptionsPage = options.OptionsPage;

  /**
   * ConsumerManagementOverlay class
   * Dialog that allows users to enroll/unenroll consumer management service.
   * @extends {SettingsDialog}
   * @constructor
   */
  function ConsumerManagementOverlay() {
    OptionsPage.call(
        this, 'consumer-management-overlay',
        loadTimeData.getString('consumerManagementOverlayTabTitle'),
        'consumer-management-overlay');

    $('consumer-management-overlay-enroll').onclick = function(event) {
      chrome.send('enrollConsumerManagement');
      OptionsPage.closeOverlay();
    };
    $('consumer-management-overlay-unenroll').onclick = function(event) {
      chrome.send('unenrollConsumerManagement');
      OptionsPage.closeOverlay();
    };
    $('consumer-management-overlay-enroll-cancel').onclick = function(event) {
      OptionsPage.closeOverlay();
    };
    $('consumer-management-overlay-unenroll-cancel').onclick = function(event) {
      OptionsPage.closeOverlay();
    };
  }

  cr.addSingletonGetter(ConsumerManagementOverlay);

  ConsumerManagementOverlay.prototype = {
    __proto__: OptionsPage.prototype,
  };

  /**
   * Shows enrollment or unenrollment content based on the enrollment status.
   * @param {boolean} isEnrolled Whether the device is enrolled.
   */
  ConsumerManagementOverlay.setEnrollmentStatus = function(isEnrolled) {
    $('unenroll-content').hidden = !($('enroll-content').hidden = isEnrolled);
  };

  // Export
  return {
    ConsumerManagementOverlay: ConsumerManagementOverlay
  };
});
