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
        startGetPrivetDestinations: function() {},
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

      print_preview.PreviewArea.prototype.getPluginType_ =
          function() {
        return print_preview.PreviewArea.PluginType_.NONE;
      };
    }.bind(this));
  },

  setUpPreview: function() {
    var initialSettingsSetEvent =
        new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
    initialSettingsSetEvent.initialSettings = this.initialSettings_;
    this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

    var localDestsSetEvent =
        new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
    localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
    this.nativeLayer_.dispatchEvent(localDestsSetEvent);
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
      false /*isInAppKioskMode*/,
      false /*hidePrintWithSystemDialogLink*/,
      ',' /*thousandsDelimeter*/,
      '.' /*decimalDelimeter*/,
      1 /*unitType*/,
      true /*isDocumentModifiable*/,
      'title' /*documentTitle*/,
      true /*documentHasSelection*/,
      false /*selectionOnly*/,
      'FooDevice' /*systemDefaultDestinationId*/,
      null /*serializedAppStateStr*/,
      false /*documentHasSelection*/);
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
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
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
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var cloudPrintEnableEvent =
      new Event(print_preview.NativeLayer.EventType.CLOUD_PRINT_ENABLE);
  cloudPrintEnableEvent.baseCloudPrintUrl = 'cloudprint url';
  this.nativeLayer_.dispatchEvent(cloudPrintEnableEvent);

  var searchDoneEvent =
      new Event(cloudprint.CloudPrintInterface.EventType.SEARCH_DONE);
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

function getCddTemplate(printerId) {
  return {
    printerId: printerId,
    capabilities: {
      version: '1.0',
      printer: {
        supported_content_type: [{content_type: 'application/pdf'}],
        collate: {},
        color: {
          option: [
            {type: 'STANDARD_COLOR', is_default: true},
            {type: 'STANDARD_MONOCHROME'}
          ]
        },
        copies: {},
        duplex: {
          option: [
            {type: 'NO_DUPLEX', is_default: true},
            {type: 'LONG_EDGE'},
            {type: 'SHORT_EDGE'}
          ]
        },
        page_orientation: {
          option: [
            {type: 'PORTRAIT', is_default: true},
            {type: 'LANDSCAPE'},
            {type: 'AUTO'}
          ]
        },
        media_size: {
          option: [
            { name: 'NA_LETTER',
              width_microns: 215900,
              height_microns: 279400,
              is_default: true
            }
          ]
        }
      }
    }
  };
}

// Test that disabled settings hide the disabled sections.
TEST_F('PrintPreviewWebUITest', 'TestSectionsDisabled', function() {
  checkSectionVisible($('layout-settings'), false);
  checkSectionVisible($('color-settings'), false);
  checkSectionVisible($('copies-settings'), false);

  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_COLOR"}
    ]
  };
  delete capsSetEvent.settingsInfo.capabilities.printer.copies;
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('layout-settings'), true);
  checkSectionVisible($('color-settings'), false);
  checkSectionVisible($('copies-settings'), false);
});

// When the source is 'PDF' and 'Save as PDF' option is selected, we hide the
// fit to page option.
TEST_F('PrintPreviewWebUITest', 'PrintToPDFSelectedCapabilities', function() {
  // Add PDF printer.
  this.initialSettings_.isDocumentModifiable_ = false;
  this.initialSettings_.systemDefaultDestinationId_ = 'Save as PDF';

  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = {
    printerId: 'Save as PDF',
    capabilities: {
      version: '1.0',
      printer: {
        page_orientation: {
          option: [
            {type: 'AUTO', is_default: true},
            {type: 'PORTRAIT'},
            {type: 'LANDSCAPE'}
          ]
        },
        color: {
          option: [
            {type: 'STANDARD_COLOR', is_default: true}
          ]
        },
        media_size: {
          option: [
            { name: 'NA_LETTER',
              width_microns: 0,
              height_microns: 0,
              is_default: true
            }
          ]
        }
      }
    }
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('other-options-settings'), false);
  checkSectionVisible($('media-size-settings'), false);
});

// When the source is 'HTML', we always hide the fit to page option and show
// media size option.
TEST_F('PrintPreviewWebUITest', 'SourceIsHTMLCapabilities', function() {
  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.fit-to-page-container'),
      false);
  checkSectionVisible($('media-size-settings'), true);
});

// When the source is "PDF", depending on the selected destination printer, we
// show/hide the fit to page option and hide media size selection.
TEST_F('PrintPreviewWebUITest', 'SourceIsPDFCapabilities', function() {
  this.initialSettings_.isDocumentModifiable_ = false;

  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.fit-to-page-container'),
      true);
  expectTrue(
      $('other-options-settings').querySelector('.fit-to-page-checkbox').
          checked);
  checkSectionVisible($('media-size-settings'), true);
});

// When the print scaling is disabled for the source "PDF", we show the fit
// to page option but the state is unchecked by default.
TEST_F('PrintPreviewWebUITest', 'PrintScalingDisabledForPlugin', function() {
  this.initialSettings_.isDocumentModifiable_ = false;

  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
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

// Make sure that custom margins controls are properly set up.
TEST_F('PrintPreviewWebUITest', 'CustomMarginsControlsCheck', function() {
  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);

  ['left', 'top', 'right', 'bottom'].forEach(function(margin) {
    var control = $('preview-area').querySelector('.margin-control-' + margin);
    assertNotEquals(null, control);
    var input = control.querySelector('.margin-control-textbox');
    assertTrue(input.hasAttribute('aria-label'));
    assertNotEquals('undefined', input.getAttribute('aria-label'));
  });
});

// Page layout has zero margins. Hide header and footer option.
TEST_F('PrintPreviewWebUITest', 'PageLayoutHasNoMarginsHideHeaderFooter',
    function() {
  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
      new print_preview.Margins(0, 0, 0, 0));

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      false);
});

// Page layout has half-inch margins. Show header and footer option.
TEST_F('PrintPreviewWebUITest', 'PageLayoutHasMarginsShowHeaderFooter',
    function() {
  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
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
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
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
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
      new print_preview.Margins(0, 36, 36, 36));

  checkElementDisplayed(
      $('other-options-settings').querySelector('.header-footer-container'),
      true);
});

// Test that the color settings, one option, standard monochrome.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsMonochrome', function() {
  this.setUpPreview();

  // Only one option, standard monochrome.
  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_MONOCHROME"}
    ]
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), false);
});

// Test that the color settings, one option, custom monochrome.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsCustomMonochrome',
    function() {
  this.setUpPreview();

  // Only one option, standard monochrome.
  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "CUSTOM_MONOCHROME", "vendor_id": "42"}
    ]
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), false);
});

// Test that the color settings, one option, standard color.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsColor', function() {
  this.setUpPreview();

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_COLOR"}
    ]
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), false);
});

// Test that the color settings, one option, custom color.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsCustomColor', function() {
  this.setUpPreview();

  var capsSetEvent =
     new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "CUSTOM_COLOR", "vendor_id": "42"}
    ]
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), false);
});

// Test that the color settings, two options, both standard, defaults to color.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsBothStandardDefaultColor',
    function() {
  this.setUpPreview();

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"type": "STANDARD_MONOCHROME"},
      {"is_default": true, "type": "STANDARD_COLOR"}
    ]
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), true);
  expectTrue($('color-settings').querySelector('.color-option').checked);
  expectFalse($('color-settings').querySelector('.bw-option').checked);
});

// Test that the color settings, two options, both standard, defaults to
// monochrome.
TEST_F('PrintPreviewWebUITest',
    'TestColorSettingsBothStandardDefaultMonochrome', function() {
  this.setUpPreview();

  var capsSetEvent =
     new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_MONOCHROME"},
      {"type": "STANDARD_COLOR"}
    ]
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), true);
  expectFalse($('color-settings').querySelector('.color-option').checked);
  expectTrue($('color-settings').querySelector('.bw-option').checked);
});

// Test that the color settings, two options, both custom, defaults to color.
TEST_F('PrintPreviewWebUITest',
    'TestColorSettingsBothCustomDefaultColor', function() {
  this.setUpPreview();

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"type": "CUSTOM_MONOCHROME", "vendor_id": "42"},
      {"is_default": true, "type": "CUSTOM_COLOR", "vendor_id": "43"}
    ]
  };
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible($('color-settings'), true);
  expectTrue($('color-settings').querySelector('.color-option').checked);
  expectFalse($('color-settings').querySelector('.bw-option').checked);
});

// Test to verify that duplex settings are set according to the printer
// capabilities.
TEST_F('PrintPreviewWebUITest', 'TestDuplexSettingsTrue', function() {
  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var otherOptionsDiv = $('other-options-settings');
  var duplexDiv = otherOptionsDiv.querySelector('.duplex-container');
  var duplexCheckbox = otherOptionsDiv.querySelector('.duplex-checkbox');

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible(otherOptionsDiv, true);
  expectFalse(duplexDiv.hidden);
  expectFalse(duplexCheckbox.checked);
});

// Test to verify that duplex settings are set according to the printer
// capabilities.
TEST_F('PrintPreviewWebUITest', 'TestDuplexSettingsFalse', function() {
  var initialSettingsSetEvent =
     new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
     new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var otherOptionsDiv = $('other-options-settings');
  var duplexDiv = otherOptionsDiv.querySelector('.duplex-container');

  var capsSetEvent =
     new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  delete capsSetEvent.settingsInfo.capabilities.printer.duplex;
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  checkSectionVisible(otherOptionsDiv, true);
  expectTrue(duplexDiv.hidden);
});

// Test that changing the selected printer updates the preview.
TEST_F('PrintPreviewWebUITest', 'TestPrinterChangeUpdatesPreview', function() {

  var initialSettingsSetEvent =
      new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
  initialSettingsSetEvent.initialSettings = this.initialSettings_;
  this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);

  var localDestsSetEvent =
      new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
  localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
  this.nativeLayer_.dispatchEvent(localDestsSetEvent);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("FooDevice");
  this.nativeLayer_.dispatchEvent(capsSetEvent);

  var previewGenerator = mock(print_preview.PreviewGenerator);
  printPreview.previewArea_.previewGenerator_ = previewGenerator.proxy();
  previewGenerator.expects(exactly(6)).requestPreview();

  var barDestination;
  var destinations = printPreview.destinationStore_.destinations();
  for (var destination, i = 0; destination = destinations[i]; i++) {
    if (destination.id == 'BarDevice') {
      barDestination = destination;
      break;
    }
  }

  printPreview.destinationStore_.selectDestination(barDestination);

  var capsSetEvent =
      new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
  capsSetEvent.settingsInfo = getCddTemplate("BarDevice");
  capsSetEvent.settingsInfo.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_MONOCHROME"}
    ]
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
