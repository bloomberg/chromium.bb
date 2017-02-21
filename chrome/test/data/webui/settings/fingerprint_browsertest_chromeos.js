// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @implements {settings.FingerprintBrowserProxy}
 * @extends {settings.TestBrowserProxy}
 */
var TestFingerprintBrowserProxy = function() {
  settings.TestBrowserProxy.call(this, [
    'getFingerprintsList',
    'getNumFingerprints',
    'startEnroll',
    'cancelCurrentEnroll',
    'getEnrollmentLabel',
    'removeEnrollment',
    'changeEnrollmentLabel',
  ]);

  /** @private {!Array<string>} */
  this.fingerprintsList_ = [];
};

TestFingerprintBrowserProxy.prototype = {
  __proto__: settings.TestBrowserProxy.prototype,

  /** @ param {!Array<string>} fingerprints */
  setFingerprints: function(fingerprints) {
    this.fingerprintsList_ = fingerprints;
  },

  /**
   * @param {settings.FingerprintResultType} result
   * @param {boolean} complete
   */
  scanReceived: function(result, complete) {
    if (complete)
      this.fingerprintsList_.push('New Label');

    cr.webUIListenerCallback('on-scan-received', {
      result: result, isComplete: complete
    });
  },

  /** @override */
  getFingerprintsList: function() {
    this.methodCalled('getFingerprintsList');
    /** @type {settings.FingerprintInfo} */
    var fingerprintInfo = {fingerprintsList: this.fingerprintsList_,
        isMaxed: this.fingerprintsList_.length >= 5};
    return Promise.resolve(fingerprintInfo);
  },

  /** @override */
  getNumFingerprints: function() {
    this.methodCalled('getNumFingerprints');
    return Promise.resolve(fingerprintsList_.length);
  },

  /** @override */
  startEnroll: function () {
    this.methodCalled('startEnroll');
  },

  /** @override */
  cancelCurrentEnroll: function() {
    this.methodCalled('cancelCurrentEnroll');
  },

  /** @override */
  getEnrollmentLabel: function(index) {
    this.methodCalled('getEnrollmentLabel');
    return Promise.resolve(this.fingerprintsList_[index]);
  },

  /** @override */
  removeEnrollment: function(index) {
    this.fingerprintsList_.splice(index, 1);
    this.methodCalled('removeEnrollment', index);
    return Promise.resolve(true);
  },

  /** @override */
  changeEnrollmentLabel: function(index, newLabel) {
    this.fingerprintsList_[index] = newLabel;
    this.methodCalled('changeEnrollmentLabel', index, newLabel);
  },
};

suite('settings-fingerprint-list', function() {
  /** @type {?SettingsFingerprintListElement} */
  var fingerprintList = null;

  /** @type {?SettingsSetupFingerprintDialogElement} */
  var dialog = null;
  /** @type {?HTMLButtonElement} */
  var addAnotherButton= null;
  /** @type {?settings.TestFingerprintBrowserProxy} */
  var browserProxy = null;

  /**
   * @param {number} index
   * @param {string=} opt_label
   */
  function createFakeEvent(index, opt_label) {
    return {model: {index: index, item: opt_label || ''}};
  }

  /**
   * @param {!Element} element
   */
  function isVisible(element) {
    return element.offsetWidth > 0 && element.offsetHeight > 0;
  }

  setup(function() {
    browserProxy = new TestFingerprintBrowserProxy();
    settings.FingerprintBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    fingerprintList = document.createElement('settings-fingerprint-list');
    document.body.appendChild(fingerprintList);
    dialog = fingerprintList.$.setupFingerprint;
    addAnotherButton = dialog.$.addAnotherButton;
    Polymer.dom.flush();
    return browserProxy.whenCalled('getFingerprintsList').then(function() {
      assertEquals(0, fingerprintList.fingerprints_.length);
    });
  });

  // Verify running through the enrolling workflow
  // (settings-setup-fingerprint-dialog) works as expected.
  test('EnrollingFingerprint', function() {
    assertFalse(dialog.$.dialog.open);
    MockInteractions.tap(fingerprintList.$$('.action-button'));
    return browserProxy.whenCalled('startEnroll').then(function() {
      assertTrue(dialog.$.dialog.open);
      assertEquals(0, dialog.receivedScanCount_);
      assertEquals(settings.FingerprintSetupStep.LOCATE_SCANNER, dialog.step_);
      browserProxy.scanReceived(settings.FingerprintResultType.SUCCESS, false);
      assertEquals(1, dialog.receivedScanCount_);
      assertEquals(settings.FingerprintSetupStep.MOVE_FINGER, dialog.step_);

      // Verify that by sending a scan problem, the div that contains the
      // problem message and icon should be visible.
      browserProxy.scanReceived(settings.FingerprintResultType.TOO_FAST, false);
      assertEquals(1, dialog.receivedScanCount_);
      assertEquals('visible',
          window.getComputedStyle(dialog.$.problemDiv).visibility);
      browserProxy.scanReceived(settings.FingerprintResultType.SUCCESS, false);
      assertEquals('hidden',
          window.getComputedStyle(dialog.$.problemDiv).visibility);
      browserProxy.scanReceived(settings.FingerprintResultType.SUCCESS, false);
      browserProxy.scanReceived(settings.FingerprintResultType.SUCCESS, true);
      assertEquals(settings.FingerprintSetupStep.READY, dialog.step_);

      // Verify that by tapping the continue button we should exit the dialog
      // and the fingerprint list should have one fingerprint registered.
      MockInteractions.tap(dialog.$.closeButton);
      return browserProxy.whenCalled('getFingerprintsList').then(
          function() {
            assertEquals(1, fingerprintList.fingerprints_.length);
          });
    });
  });

  // Verify enrolling a fingerprint, then enrolling another without closing the
  // dialog works as intended.
  test('EnrollingAnotherFingerprint', function() {
    assertFalse(dialog.$.dialog.open);
    MockInteractions.tap(fingerprintList.$$('.action-button'));
    return browserProxy.whenCalled('startEnroll').then(function() {
      assertTrue(dialog.$.dialog.open);
      assertEquals(0, dialog.receivedScanCount_);
      assertFalse(isVisible(addAnotherButton));
      browserProxy.scanReceived(settings.FingerprintResultType.SUCCESS, true);
      assertEquals(settings.FingerprintSetupStep.READY, dialog.step_);

      // Once the first fingerprint is enrolled, verify that enrolling the
      // second fingerprint without closing the dialog works as expected.
      return browserProxy.whenCalled('getFingerprintsList');
    }).then(function() {
      assertEquals(1, fingerprintList.fingerprints_.length);
      assertTrue(dialog.$.dialog.open);
      assertTrue(isVisible(addAnotherButton));
      MockInteractions.tap(addAnotherButton);
      return browserProxy.whenCalled('startEnroll');
    }).then(function() {
      assertTrue(dialog.$.dialog.open);
      assertFalse(isVisible(addAnotherButton));
      browserProxy.scanReceived(settings.FingerprintResultType.SUCCESS, true);
      return browserProxy.whenCalled('getFingerprintsList');
    }).then(function() {
       assertEquals(2, fingerprintList.fingerprints_.length);
    });
  });

  test('CancelEnrollingFingerprint', function() {
    assertFalse(dialog.$.dialog.open);
    MockInteractions.tap(fingerprintList.$$('.action-button'));
    return browserProxy.whenCalled('startEnroll').then(function() {
      assertTrue(dialog.$.dialog.open);
      assertEquals(0, dialog.receivedScanCount_);
      assertEquals(settings.FingerprintSetupStep.LOCATE_SCANNER, dialog.step_);
      browserProxy.scanReceived(settings.FingerprintResultType.SUCCESS, false);
      assertEquals(1, dialog.receivedScanCount_);
      assertEquals(settings.FingerprintSetupStep.MOVE_FINGER, dialog.step_);

      // Verify that by tapping the exit button we should exit the dialog
      // and the fingerprint list should have zero fingerprints registered.
      MockInteractions.tap(dialog.$.closeButton);
      return browserProxy.whenCalled('cancelCurrentEnroll');
    }).then(function() {
      assertEquals(0, fingerprintList.fingerprints_.length);
    });
  });

  test('RemoveFingerprint', function() {
    browserProxy.setFingerprints(['Label 1', 'Label 2']);
    fingerprintList.updateFingerprintsList_();

    return browserProxy.whenCalled('getFingerprintsList').then(function() {
      assertEquals(2, fingerprintList.fingerprints_.length);
      fingerprintList.onFingerprintDeleteTapped_(createFakeEvent(0));

      return Promise.all([
          browserProxy.whenCalled('removeEnrollment'),
          browserProxy.whenCalled('getFingerprintsList')]);
    }).then(function() {
      assertEquals(1, fingerprintList.fingerprints_.length);
    });
  });

  test('ChangeFingerprintLabel', function() {
    browserProxy.setFingerprints(['Label 1']);
    fingerprintList.updateFingerprintsList_();

    return browserProxy.whenCalled('getFingerprintsList').then(function() {
      assertEquals(1, fingerprintList.fingerprints_.length);
      assertEquals('Label 1', fingerprintList.fingerprints_[0]);

      // Verify that by sending a fingerprint input change event, the new
      // label gets changed as expected.
      fingerprintList.onFingerprintLabelChanged_(
          createFakeEvent(0, 'New Label 1'));
      return browserProxy.whenCalled('changeEnrollmentLabel');
    }).then(function() {
      assertEquals('New Label 1', fingerprintList.fingerprints_[0]);
    });
  });

  test('AddingNewFingerprint', function() {
    browserProxy.setFingerprints(['1', '2', '3', '4', '5']);
    fingerprintList.updateFingerprintsList_();

    // Verify that new fingerprints cannot be added when there are already five
    // registered fingerprints.
    return browserProxy.whenCalled('getFingerprintsList').then(function() {
      assertEquals(5, fingerprintList.fingerprints_.length);
      assertTrue(fingerprintList.$$('.action-button').disabled);
      fingerprintList.onFingerprintDeleteTapped_(createFakeEvent(0));

      return Promise.all([
          browserProxy.whenCalled('removeEnrollment'),
          browserProxy.whenCalled('getFingerprintsList')]);
    }).then(function() {
      assertEquals(4, fingerprintList.fingerprints_.length);
      assertFalse(
          fingerprintList.$$('.action-button').disabled);
    });
  });
});
