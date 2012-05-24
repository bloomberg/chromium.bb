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
 * Index of the Foo printer.
 * @type {number}
 * @const
 */
PrintPreviewWebUITest.FOO_INDEX = 0;

/**
 * Index of the Bar printer.
 * @type {number}
 * @const
 */
PrintPreviewWebUITest.BAR_INDEX = 1;

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

      this.nativeLayer_ = new NativeLayerStub();
      printPreview.nativeLayer_ = this.nativeLayer_;

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
      0 /*marginsType*/,
      null /*customMargins*/,
      true /*isDuplexEnabled*/,
      false /*isHeaderFooterEnabled*/,
      'FooDevice' /*initialDestinationId*/);
    this.localDestinationInfos_ = [
      { printerName: 'FooName', deviceName: 'FooDevice' },
      { printerName: 'BarName', deviceName: 'BarDevice' }
    ];
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

  var printerList = $('destination-settings').
      getElementsByClassName('destination-settings-select')[0];
  assertNotEquals(null, printerList);
  assertGE(printerList.options.length, 2);
  expectEquals(PrintPreviewWebUITest.FOO_INDEX, printerList.selectedIndex);
  expectEquals('FooName',
               printerList.options[PrintPreviewWebUITest.FOO_INDEX].text,
               'fooIndex=' + PrintPreviewWebUITest.FOO_INDEX);
  expectEquals('FooDevice',
               printerList.options[PrintPreviewWebUITest.FOO_INDEX].value,
               'fooIndex=' + PrintPreviewWebUITest.FOO_INDEX);
  expectEquals('BarName',
               printerList.options[PrintPreviewWebUITest.BAR_INDEX].text,
               'barIndex=' + PrintPreviewWebUITest.BAR_INDEX);
  expectEquals('BarDevice',
               printerList.options[PrintPreviewWebUITest.BAR_INDEX].value,
               'barIndex=' + PrintPreviewWebUITest.BAR_INDEX);
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

  var printerList = $('destination-settings').
      getElementsByClassName('destination-settings-select')[0];
  assertNotEquals(null, printerList);
  assertGE(printerList.options.length, 2);
  expectEquals(PrintPreviewWebUITest.FOO_INDEX, printerList.selectedIndex);
  expectEquals('FooName',
               printerList.options[PrintPreviewWebUITest.FOO_INDEX].text,
               'fooIndex=' + PrintPreviewWebUITest.FOO_INDEX);
  expectEquals('FooDevice',
               printerList.options[PrintPreviewWebUITest.FOO_INDEX].value,
               'fooIndex=' + PrintPreviewWebUITest.FOO_INDEX);
  expectEquals('BarName',
               printerList.options[PrintPreviewWebUITest.BAR_INDEX].text,
               'barIndex=' + PrintPreviewWebUITest.BAR_INDEX);
  expectEquals('BarDevice',
               printerList.options[PrintPreviewWebUITest.BAR_INDEX].value,
               'barIndex=' + PrintPreviewWebUITest.BAR_INDEX);
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
  expectEquals(isDisplayed, el.style.display != 'none');
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
  this.initialSettings_.initialDestinationId_ = 'Save as PDF';
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-fit-to-page'),
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-fit-to-page'),
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-fit-to-page'),
      true);
  expectTrue(
      $('other-options-settings').querySelector(
          '.other-options-settings-fit-to-page-checkbox').checked);
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
      $('other-options-settings').
          querySelector('.other-options-settings-fit-to-page'),
      true);
  expectFalse(
      $('other-options-settings').querySelector(
          '.other-options-settings-fit-to-page-checkbox').checked);
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(0, 0, 0, 0));

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(36, 36, 36, 36));

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(0, 36, 0, 36));

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
      true);

  printPreview.printTicketStore_.updateMarginsType(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.updateCustomMargins(
      new print_preview.Margins(0, 36, 36, 36));

  checkElementDisplayed(
      $('other-options-settings').
          querySelector('.other-options-settings-header-footer'),
      true);
});

// Test that the color settings are set according to the printer capabilities.
TEST_F('PrintPreviewWebUITest', 'TestColorSettings', function() {
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
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': true,
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

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'disableColorOption': false,
    'setColorAsDefault': false,
    'disableCopiesOption': false,
    'disableLandscapeOption': false,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), true);
  expectFalse(colorOption.checked);
  expectTrue(bwOption.checked);
});

// Test to verify that duplex settings are set according to the printer
// capabilities.
TEST_F('PrintPreviewWebUITest', 'TestDuplexSettings', function() {
  var initialSettingsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  // Need to override test defaults for the initial settings, because initial
  // duplex value needs to be unspecified.
  initialSettingsSetEvent.initialSettings =
      new print_preview.NativeInitialSettings(
          false /*isInKioskAutoPrintMode*/,
          ',' /*thousandsDelimeter*/,
          '.' /*decimalDelimeter*/,
          1 /*unitType*/,
          true /*isDocumentModifiable*/,
          0 /*marginsType*/,
          null /*customMargins*/,
          null /*isDuplexEnabled*/,
          false /*isHeaderFooterEnabled*/,
          'FooDevice' /*initialDestinationId*/);
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var copiesDiv = $('copies-settings');
  var duplexDiv = copiesDiv.getElementsByClassName('copies-settings-duplex')[0];
  var duplexCheckbox = copiesDiv.getElementsByClassName(
      'copies-settings-duplex-checkbox')[0];

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'disableColorOption': false,
    'setColorAsDefault': true,
    'disableCopiesOption': false,
    'disableLandscapeOption': true,
    'printerDefaultDuplexValue': 0
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible(copiesDiv, true);
  expectFalse(duplexDiv.hidden);
  expectFalse(duplexCheckbox.checked);

  // If the printer default duplex value is UNKNOWN_DUPLEX_MODE, hide the
  // two sided option.
  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'disableColorOption': false,
    'setColorAsDefault': false,
    'disableCopiesOption': false,
    'disableLandscapeOption': false,
    'printerDefaultDuplexValue': -1
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible(copiesDiv, true);
  expectTrue(duplexDiv.hidden);

  var capsSetEvent =
      new cr.Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    'disableColorOption': false,
    'setColorAsDefault': false,
    'disableCopiesOption': false,
    'disableLandscapeOption': false,
    'printerDefaultDuplexValue': 1
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible(copiesDiv, true);
  expectFalse(duplexDiv.hidden);
  expectTrue(duplexCheckbox.checked);
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
  expectEquals('none', loadingMessageEl.style.display);

  var previewFailedMessageEl = previewAreaEl.getElementsByClassName(
      'preview-area-preview-failed-message')[0];
  expectEquals('none', previewFailedMessageEl.style.display);

  var printFailedMessageEl =
      previewAreaEl.getElementsByClassName('preview-area-print-failed')[0];
  expectEquals('none', printFailedMessageEl.style.display);

  var customMessageEl =
      previewAreaEl.getElementsByClassName('preview-area-custom-message')[0];
  expectEquals('', customMessageEl.style.display);
});
