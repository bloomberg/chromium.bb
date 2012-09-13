// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for print preview WebUI testing.
 * @constructor
 * @extends {testing.Test}
 */
function PrintPreviewWebUITest() {
  testing.Test.call(this);
  this.nativeLayer_ = null;
  this.initialSettings_ = null;
  this.localDestinationInfos_ = null;
}

/**
 * Index of the "Save as PDF" printer.
 * @type {number}
 * @const
 */
PrintPreviewWebUITest.PDF_INDEX = 0;

/**
 * Index of the Foo printer.
 * @type {number}
 * @const
 */
PrintPreviewWebUITest.FOO_INDEX = 1;

/**
 * Index of the Bar printer.
 * @type {number}
 * @const
 */
PrintPreviewWebUITest.BAR_INDEX = 2;

PrintPreviewWebUITest.prototype = {
  __proto__: testing.Test.prototype,

  /**
   * Browse to the sample page, cause print preview & call preLoad().
   * @type {string}
   * @override
   */
  browsePrintPreload: 'print_preview_hello_world_test.html',

  /**
   * Stub out low-level functionality like the NativeLayer and
   * CloudPrintInterface.
   * @this {PrintPreviewWebUITest}
   * @override
   */
  preLoad: function() {
    window.addEventListener('DOMContentLoaded', function() {
      function NativeLayerStub() {
        cr.EventTarget.call(this);
      }
      NativeLayerStub.prototype = {
        __proto__: cr.EventTarget.prototype,
        startGetInitialSettings: function() {},
        startGetLocalDestinations: function() {},
        startGetLocalDestinationCapabilities: function(destinationId) {}
      };
      var oldNativeLayerEventType = print_preview.NativeLayer.EventType;
      var oldDuplexMode = print_preview.NativeLayer.DuplexMode;
      print_preview.NativeLayer = NativeLayerStub;
      print_preview.NativeLayer.EventType = oldNativeLayerEventType;
      print_preview.NativeLayer.DuplexMode = oldDuplexMode;

      function CloudPrintInterfaceStub() {
        cr.EventTarget.call(this);
      }
      CloudPrintInterfaceStub.prototype = {
        __proto__: cr.EventTarget.prototype,
        search: function(isRecent) {}
      };
      var oldCpInterfaceEventType = cloudprint.CloudPrintInterface.EventType;
      cloudprint.CloudPrintInterface = CloudPrintInterfaceStub;
      cloudprint.CloudPrintInterface.EventType = oldCpInterfaceEventType;

      print_preview.PreviewArea.prototype.checkPluginCompatibility_ =
          function() {
        return false;
      };
    }.bind(this));
  },

  /**
   * Generate a real C++ class; don't typedef.
   * @type {?string}
   * @override
   */
  typedefCppFixture: null,

  /**
   * @this {PrintPreviewWebUITest}
   * @override
   */
  setUp: function() {
    Mock4JS.clearMocksToVerify();

    this.initialSettings_ = new print_preview.NativeInitialSettings(
      false /*isInKioskAutoPrintMode*/,
      ',' /*thousandsDelimeter*/,
      '.' /*decimalDelimeter*/,
      1 /*unitType*/,
      true /*isDocumentModifiable*/,
      'title' /*documentTitle*/,
      'FooDevice' /*systemDefaultDestinationId*/,
      null /*serializedAppStateStr*/);
    this.localDestinationInfos_ = [
      { printerName: 'FooName', deviceName: 'FooDevice' },
      { printerName: 'BarName', deviceName: 'BarDevice' }
    ];
    this.nativeLayer_ = printPreview.nativeLayer_;
  }
};

GEN('#include "chrome/test/data/webui/print_preview.h"');

// Test some basic assumptions about the print preview WebUI.
TEST_F('PrintPreviewWebUITest', 'TestPrinterList', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var recentList = $('destination-search').querySelector('.recent-list ul');
  var localList = $('destination-search').querySelector('.local-list ul');
  assertNotEquals(null, recentList);
  assertEquals(1, recentList.childNodes.length);
  assertEquals('FooName',
               recentList.childNodes.item(0).querySelector(
                   '.destination-list-item-name').textContent);

  assertNotEquals(null, localList);
  assertEquals(3, localList.childNodes.length);
  assertEquals('Save as PDF',
               localList.childNodes.item(PrintPreviewWebUITest.PDF_INDEX).
                   querySelector('.destination-list-item-name').textContent);
  assertEquals('FooName',
               localList.childNodes.item(PrintPreviewWebUITest.FOO_INDEX).
                   querySelector('.destination-list-item-name').textContent);
  assertEquals('BarName',
               localList.childNodes.item(PrintPreviewWebUITest.BAR_INDEX).
                   querySelector('.destination-list-item-name').textContent);
});

// Test that the printer list is structured correctly after calling
// addCloudPrinters with an empty list.
TEST_F('PrintPreviewWebUITest', 'TestPrinterListCloudEmpty', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var cloudPrintEnableEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CLOUD_PRINT_ENABLE);
  cloudPrintEnableEvent.baseCloudPrintUrl = 'cloudprint url';
  this.nativeLayer_.dispatchEvent(cloudPrintEnableEvent);

  var searchDoneEvent =
      new cr.Event(cloudprint.CloudPrintInterface.EventType.SEARCH_DONE);
  searchDoneEvent.printers = [];
  searchDoneEvent.isRecent = true;
  searchDoneEvent.email = 'foo@chromium.org';
  printPreview.cloudPrintInterface_.dispatchEvent(searchDoneEvent);

  var recentList = $('destination-search').querySelector('.recent-list ul');
  var localList = $('destination-search').querySelector('.local-list ul');
  var cloudList = $('destination-search').querySelector('.cloud-list ul');

  assertNotEquals(null, recentList);
  assertEquals(1, recentList.childNodes.length);
  assertEquals('FooName',
               recentList.childNodes.item(0).querySelector(
                   '.destination-list-item-name').textContent);

  assertNotEquals(null, localList);
  assertEquals(3, localList.childNodes.length);
  assertEquals('Save as PDF',
               localList.childNodes.item(PrintPreviewWebUITest.PDF_INDEX).
                   querySelector('.destination-list-item-name').textContent);
  assertEquals('FooName',
               localList.childNodes.item(PrintPreviewWebUITest.FOO_INDEX).
                   querySelector('.destination-list-item-name').textContent);
  assertEquals('BarName',
               localList.childNodes.item(PrintPreviewWebUITest.BAR_INDEX).
                   querySelector('.destination-list-item-name').textContent);

  assertNotEquals(null, cloudList);
  assertEquals(0, cloudList.childNodes.length);
});

/**
 * Verify that |section| visibility matches |visible|.
 * @param {HTMLDivElement} section The section to check.
 * @param {boolean} visible The expected state of visibility.
 */
function checkSectionVisible(section, visible) {
  assertNotEquals(null, section);
  expectEquals(
      visible, section.classList.contains('visible'), 'section=' + section.id);
}

function checkElementDisplayed(el, isDisplayed) {
  assertNotEquals(null, el);
  expectEquals(isDisplayed, !el.hidden);
}

// Test that disabled settings hide the disabled sections.
TEST_F('PrintPreviewWebUITest', 'TestSectionsDisabled', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  checkSectionVisible($('layout-settings'), true);
  checkSectionVisible($('color-settings'), true);
  checkSectionVisible($('copies-settings'), true);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': true,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('layout-settings'), false);
  checkSectionVisible($('color-settings'), false);
  checkSectionVisible($('copies-settings'), false);
});

// When the source is 'PDF' and 'Save as PDF' option is selected, we hide the
// fit to page option.
TEST_F('PrintPreviewWebUITest',
       'PrintToPDFSelectedHideFitToPageOption',
       function() {
  // Add PDF printer.
  this.initialSettings_.isDocumentModifiable_ = false;
  this.initialSettings_.systemDefaultDestinationId_ = 'Save as PDF';
  this.localDestinationInfos_.push(
      {printerName: 'Save as PDF', deviceName: 'Save as PDF'});

  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.fit-to-page-container'),
      false);
});

// When the source is 'HTML', we always hide the fit to page option.
TEST_F('PrintPreviewWebUITest', 'SourceIsHTMLHideFitToPageOption', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.fit-to-page-container'),
      false);
});

// When the source is "PDF", depending on the selected destination printer, we
// show/hide the fit to page option.
TEST_F('PrintPreviewWebUITest', 'SourceIsPDFShowFitToPageOption', function() {
  this.initialSettings_.isDocumentModifiable_ = false;

  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.fit-to-page-container'),
      true);
  expectTrue(
      $('other-options-settings').querySelector('.fit-to-page-checkbox').
          checked);
});

// When the print scaling is disabled for the source "PDF", we show the fit
// to page option but the state is unchecked by default.
TEST_F('PrintPreviewWebUITest', 'PrintScalingDisabledForPlugin', function() {
  this.initialSettings_.isDocumentModifiable_ = false;

  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  // Indicate that the PDF does not support scaling by default.
  cr.dispatchSimpleEvent(
      this.nativeLayer_, print_preview.NativeLayer.EventType.DISABLE_SCALING);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.fit-to-page-container'),
      true);
  expectFalse(
      $('other-options-settings').querySelector('.fit-to-page-checkbox').
          checked);
});

// Page layout has zero margins. Hide header and footer option.
TEST_F('PrintPreviewWebUITest',
       'PageLayoutHasNoMarginsHideHeaderFooter',
       function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(0, 0, 0, 0));

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      false);
});

// Page layout has half-inch margins. Show header and footer option.
TEST_F('PrintPreviewWebUITest',
       'PageLayoutHasMarginsShowHeaderFooter',
       function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(36, 36, 36, 36));

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);
});

// Page layout has zero top and bottom margins. Hide header and footer option.
TEST_F('PrintPreviewWebUITest',
       'ZeroTopAndBottomMarginsHideHeaderFooter',
       function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(0, 36, 0, 36));

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      false);
});

// Page layout has zero top and half-inch bottom margin. Show header and footer
// option.
TEST_F('PrintPreviewWebUITest',
       'ZeroTopAndNonZeroBottomMarginShowHeaderFooter',
       function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(0, 36, 36, 36));

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);
});

// Test that the color settings are set according to the printer capabilities.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsTrue', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  checkSectionVisible($('color-settings'), true);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': false,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), true);

  var colorOption = $('color-settings').getElementsByClassName(
      'color-settings-color-option')[0];
  var bwOption = $('color-settings').getElementsByClassName(
      'color-settings-bw-option')[0];
  expectTrue(colorOption.checked);
  expectFalse(bwOption.checked);
});

//Test that the color settings are set according to the printer capabilities.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsFalse', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  checkSectionVisible($('color-settings'), true);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': true,
    'setColorAsDefault': false,
    'disableCopiesOption': false,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), false);

  var colorOption = $('color-settings').getElementsByClassName(
      'color-settings-color-option')[0];
  var bwOption = $('color-settings').getElementsByClassName(
      'color-settings-bw-option')[0];
  expectFalse(colorOption.checked);
  expectTrue(bwOption.checked);
});

// Test to verify that duplex settings are set according to the printer
// capabilities.
TEST_F('PrintPreviewWebUITest', 'TestDuplexSettingsTrue', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var otherOptionsDiv = $('other-options-settings');
  var duplexDiv = otherOptionsDiv.querySelector('.duplex-container');
  var duplexCheckbox = otherOptionsDiv.querySelector('.duplex-checkbox');

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': false,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0,
    'setDuplexAsDefault': false
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible(otherOptionsDiv, true);
  expectFalse(duplexDiv.hidden);
  expectFalse(duplexCheckbox.checked);
});

//Test to verify that duplex settings are set according to the printer
//capabilities.
TEST_F('PrintPreviewWebUITest', 'TestDuplexSettingsFalse', function() {
  var initialSettingsSetEvent =
     new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
     new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var otherOptionsDiv = $('other-options-settings');
  var duplexDiv = otherOptionsDiv.querySelector('.duplex-container');

  var capsSetEvent =
     new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
   'printerId': 'FooDevice',
   'disableColorOption': false,
   'setColorAsDefault': true,
   'disableCopiesOption': false,
   'disableLandscapeOption': true,
   'printerDefaultDuplexValue': -1,
   'setDuplexAsDefault': false
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible(otherOptionsDiv, true);
  expectTrue(duplexDiv.hidden);
});

// Test that changing the selected printer updates the preview.
TEST_F('PrintPreviewWebUITest', 'TestPrinterChangeUpdatesPreview', function() {

  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'FooDevice',
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  var previewGenerator = mock(print_preview.PreviewGenerator);
  printPreview.previewArea_.previewGenerator_ = previewGenerator.proxy();
  previewGenerator.expects(once()).requestPreview();

  var barDestination;
  var destinations = printPreview.destinationStore_.destinations;
  for (var destination, i = 0; destination = destinations[i]; i++) {
    if (destination.id == 'BarDevice') {
      barDestination = destination;
      break;
    }
  }

  printPreview.destinationStore_.selectDestination(barDestination);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'printerId': 'BarDevice',
    'disableColorOption': true,
    'setColorAsDefault': false,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);
});

// Test that error message is displayed when plugin doesn't exist.
TEST_F('PrintPreviewWebUITest', 'TestNoPDFPluginErrorMessage', function() {
  var previewAreaEl = $('preview-area');

  var loadingMessageEl =
      previewAreaEl.getElementsByClassName('preview-area-loading-message')[0];
  expectEquals(true, loadingMessageEl.hidden);

  var previewFailedMessageEl = previewAreaEl.getElementsByClassName(
      'preview-area-preview-failed-message')[0];
  expectEquals(true, previewFailedMessageEl.hidden);

  var printFailedMessageEl =
      previewAreaEl.getElementsByClassName('preview-area-print-failed')[0];
  expectEquals(true, printFailedMessageEl.hidden);

  var customMessageEl =
      previewAreaEl.getElementsByClassName('preview-area-custom-message')[0];
  expectEquals(false, customMessageEl.hidden);
});
