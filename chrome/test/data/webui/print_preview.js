// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN('#include "base/feature_list.h"');
GEN('#include "chrome/common/chrome_features.h"');

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

  /** @override */
  runAccessibilityChecks: true,

  /** @override */
  accessibilityIssuesAreErrors: true,

  /** @override */
  isAsync: true,

  /**
   * @override
   */
  testGenPreamble: function() {
    // Enable print scaling and print as image for tests.
    GEN('  base::FeatureList::ClearInstanceForTesting();');
    GEN('  std::unique_ptr<base::FeatureList>');
    GEN('      feature_list(new base::FeatureList);');
    GEN('  char enabled_features[128] = {0};');
    GEN('  strcpy(enabled_features, features::kPrintScaling.name);');
    GEN('#if !defined(OS_WINDOWS) && !defined(OS_MACOSX)');
    GEN('  strcat(strcat(enabled_features, ","), ');
    GEN('      features::kPrintPdfAsImage.name);');
    GEN('#endif');
    GEN('  feature_list->InitializeFromCommandLine(');
    GEN('      enabled_features, std::string());');
    GEN('  base::FeatureList::SetInstance(std::move(feature_list));');
  },

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
        startGetExtensionDestinations: function() {},
        startGetLocalDestinationCapabilities: function(destinationId) {},
        startGetPreview: function() {},
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
   * Dispatch the INITIAL_SETTINGS_SET event. This call is NOT async and will
   * happen in the same thread.
   */
  setInitialSettings: function() {
    var initialSettingsSetEvent =
        new Event(print_preview.NativeLayer.EventType.INITIAL_SETTINGS_SET);
    initialSettingsSetEvent.initialSettings = this.initialSettings_;
    this.nativeLayer_.dispatchEvent(initialSettingsSetEvent);
  },

  /**
   * Dispatch the LOCAL_DESTINATIONS_SET event. This call is NOT async and will
   * happen in the same thread.
   */
  setLocalDestinations: function() {
    var localDestsSetEvent =
        new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
    localDestsSetEvent.destinationInfos = this.localDestinationInfos_;
    this.nativeLayer_.dispatchEvent(localDestsSetEvent);
  },

  /**
   * Dispatch the CAPABILITIES_SET event. This call is NOT async and will
   * happen in the same thread.
   * @device - The device whose capabilities should be dispatched.
   */
  setCapabilities: function(device) {
    var capsSetEvent =
        new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
    capsSetEvent.settingsInfo = device;
    this.nativeLayer_.dispatchEvent(capsSetEvent);
  },

  /**
   * Even though animation duration and delay is set to zero, it is necessary to
   * wait until the animation has finished.
   */
  waitForAnimationToEnd: function(elementId) {
    // add a listener for the animation end event
    document.addEventListener('animationend', function f(e) {
      if (e.target.id == elementId) {
        document.removeEventListener(f, 'animationend');
        testDone();
      }
    });
  },

  /**
   * Expand the 'More Settings' div to expose all options.
   */
  expandMoreSettings: function() {
    var moreSettings = $('more-settings');
    checkSectionVisible(moreSettings, true);
    moreSettings.click();
  },

  /**
   * Repeated setup steps for the advanced settings tests.
   * Disables accessiblity errors, sets initial settings, and verifies
   * advanced options section is visible after expanding more settings.
   */
  setupAdvancedSettingsTest: function(device) {
    // Need to disable this since overlay animation will not fully complete.
    this.accessibilityIssuesAreErrors = false;
    this.setInitialSettings();
    this.setLocalDestinations();
    this.setCapabilities(device);
    this.expandMoreSettings();

    // Check that the advanced options settings section is visible.
    checkSectionVisible($('advanced-options-settings'), true);
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
    testing.Test.prototype.setUp.call(this);
    Mock4JS.clearMocksToVerify();

    this.initialSettings_ = new print_preview.NativeInitialSettings(
      false /*isInKioskAutoPrintMode*/,
      false /*isInAppKioskMode*/,
      ',' /*thousandsDelimeter*/,
      '.' /*decimalDelimeter*/,
      1 /*unitType*/,
      true /*isDocumentModifiable*/,
      'title' /*documentTitle*/,
      true /*documentHasSelection*/,
      false /*selectionOnly*/,
      'FooDevice' /*systemDefaultDestinationId*/,
      null /*serializedAppStateStr*/,
      null /*serializedDefaultDestinationSelectionRulesStr*/);
    this.localDestinationInfos_ = [
      { printerName: 'FooName', deviceName: 'FooDevice' },
      { printerName: 'BarName', deviceName: 'BarDevice' }
    ];
    this.nativeLayer_ = printPreview.nativeLayer_;

    testing.Test.disableAnimationsAndTransitions();

    // Enable when failure is resolved.
    // AX_TEXT_03: http://crbug.com/559209
    this.accessibilityAuditConfig.ignoreSelectors(
        'multipleLabelableElementsPerLabel',
        '#page-settings > .right-column > *');
  }
};

GEN('#include "chrome/test/data/webui/print_preview.h"');

// Test some basic assumptions about the print preview WebUI.
TEST_F('PrintPreviewWebUITest', 'TestPrinterList', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

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

  testDone();
});

// Test that the printer list is structured correctly after calling
// addCloudPrinters with an empty list.
TEST_F('PrintPreviewWebUITest', 'TestPrinterListCloudEmpty', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

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

  testDone();
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
  expectEquals(isDisplayed,
               !el.hidden,
               'element="' + el.id + '" of class "' + el.classList + '"');
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

// Test restore settings with one destination.
TEST_F('PrintPreviewWebUITest', 'TestPrintPreviewRestoreLocalDestination',
    function() {
  this.initialSettings_.serializedAppStateStr_ =
      '{"version":2,"recentDestinations":[{"id":"ID", "origin":"local",' +
        '"account":"", "capabilities":0, "name":"", "extensionId":"",' +
            '"extensionName":""}]}';
  this.setInitialSettings();

  testDone();
});

//Test with multiple destinations
TEST_F('PrintPreviewWebUITest', 'TestPrintPreviewRestoreMultipleDestinations',
    function() {
  var origin = cr.isChromeOS ? "chrome_os" : "local";

  var appState = {
    'version': 2,
    'recentDestinations': [
      {
        'id': 'ID1',
        'origin': origin,
        'account': '',
        'capabilities': 0,
        'name': '',
        'extensionId': '',
        'extensionName': ''
      },
      {
        'id': 'ID2',
        'origin': origin,
        'account': '',
        'capabilities': 0,
        'name': '',
        'extensionId': '',
        'extensionName': ''
      },
      {
        'id': 'ID3',
        'origin': origin,
        'account': '',
        'capabilities': 0,
        'name': '',
        'extensionId': '',
        'extensionName': ''
      }
    ]
  };

  this.initialSettings_.serializedAppStateStr_ = JSON.stringify(appState);

  this.setInitialSettings();

  // Set capabilities for the three recently used destinations + 1 more
  this.setCapabilities(getCddTemplate('ID1'));
  this.setCapabilities(getCddTemplate('ID2'));
  this.setCapabilities(getCddTemplate('ID3'));
  this.setCapabilities(getCddTemplate('ID4'));

  // The most recently used destination should be the currently selected one.
  // This is ID1.
  assertEquals(
      'ID1', printPreview.destinationStore_.selectedDestination.id);

  // Look through the destinations. ID1, ID2, and ID3 should all be recent.
  var destinations = printPreview.destinationStore_.destinations_;
  var ids_found = [];

  for (var i = 0; i < destinations.length; i++) {
    if (!destinations[i])
      continue;
    if (destinations[i].isRecent)
      ids_found.push(destinations[i].id);
  }

  // Make sure there were 3 recent destinations and that they are the correct
  // IDs.
  assertEquals(3, ids_found.length);
  assertNotEquals(-1, ids_found.indexOf("ID1"));
  assertNotEquals(-1, ids_found.indexOf("ID2"));
  assertNotEquals(-1, ids_found.indexOf("ID3"));

  testDone();
});

TEST_F('PrintPreviewWebUITest',
    'TestPrintPreviewDefaultDestinationSelectionRules', function() {
  // It also makes sure these rules do override system default destination.
  this.initialSettings_.serializedDefaultDestinationSelectionRulesStr_ =
      '{"namePattern":".*Bar.*"}';
  this.setInitialSettings();
  this.setLocalDestinations();

  assertEquals(
      'BarDevice', printPreview.destinationStore_.selectedDestination.id);

  testDone();
});

TEST_F('PrintPreviewWebUITest', 'TestSystemDialogLinkIsHiddenInAppKioskMode',
    function() {
  if (cr.isChromeOS) {
    assertEquals(null, $('system-dialog-link'));
  } else {
    this.initialSettings_.isInAppKioskMode_ = true;
    this.setInitialSettings();

    checkElementDisplayed($('system-dialog-link'), false);
  }

  testDone();
});

// Test that disabled settings hide the disabled sections.
TEST_F('PrintPreviewWebUITest', 'TestSectionsDisabled', function() {
  checkSectionVisible($('layout-settings'), false);
  checkSectionVisible($('color-settings'), false);
  checkSectionVisible($('copies-settings'), false);

  this.setInitialSettings();
  this.setLocalDestinations();
  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_COLOR"}
    ]
  };
  delete device.capabilities.printer.copies;
  this.setCapabilities(device);

  checkSectionVisible($('layout-settings'), true);
  checkSectionVisible($('color-settings'), false);
  checkSectionVisible($('copies-settings'), false);

  this.waitForAnimationToEnd('other-options-collapsible');
});

// When the source is 'PDF' and 'Save as PDF' option is selected, we hide the
// fit to page option.
TEST_F('PrintPreviewWebUITest', 'PrintToPDFSelectedCapabilities', function() {
  // Add PDF printer.
  this.initialSettings_.isDocumentModifiable_ = false;
  this.initialSettings_.systemDefaultDestinationId_ = 'Save as PDF';
  this.setInitialSettings();

  var device = {
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
  this.setCapabilities(device);

  var otherOptions = $('other-options-settings');
  // If rasterization is an option, other options should be visible. If not,
  // there should be no available other options.
  checkSectionVisible(otherOptions, !cr.isWindows && !cr.isMac);
  if (!cr.isWindows && !cr.isMac) {
    checkElementDisplayed(
        otherOptions.querySelector('#fit-to-page-container'), false);
    checkElementDisplayed(
        otherOptions.querySelector('#rasterize-container'), true);
  }
  checkSectionVisible($('media-size-settings'), false);
  checkSectionVisible($('scaling-settings'), false);

  testDone();
});

// When the source is 'HTML', we always hide the fit to page option and show
// media size option.
TEST_F('PrintPreviewWebUITest', 'SourceIsHTMLCapabilities', function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  var fitToPage = otherOptions.querySelector('#fit-to-page-container');
  var rasterize;
  var rasterizeEnabled = !cr.isWindows && !cr.isMac;
  if (rasterizeEnabled)
    rasterize = otherOptions.querySelector('#rasterize-container');
  var mediaSize = $('media-size-settings');
  var scalingSettings = $('scaling-settings');

  // Check that options are collapsed (section is visible, because duplex is
  // available).
  checkSectionVisible(otherOptions, true);
  checkElementDisplayed(fitToPage, false);
  if (rasterizeEnabled)
    checkElementDisplayed(rasterize, false);
  checkSectionVisible(mediaSize, false);
  checkSectionVisible(scalingSettings, false);

  this.expandMoreSettings();

  checkElementDisplayed(fitToPage, false);
  if (rasterizeEnabled)
    checkElementDisplayed(rasterize, false);
  checkSectionVisible(mediaSize, true);
  checkSectionVisible(scalingSettings, true);

  this.waitForAnimationToEnd('more-settings');
});

// When the source is "PDF", depending on the selected destination printer, we
// show/hide the fit to page option and hide media size selection.
TEST_F('PrintPreviewWebUITest', 'SourceIsPDFCapabilities', function() {
  this.initialSettings_.isDocumentModifiable_ = false;
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  var scalingSettings = $('scaling-settings');
  var fitToPageContainer =
      otherOptions.querySelector('#fit-to-page-container');
  var rasterizeEnabled = !cr.isWindows && !cr.isMac;
  var rasterizeContainer;
  if (rasterizeEnabled) {
    rasterizeContainer =
      otherOptions.querySelector('#rasterize-container');
  }

  checkSectionVisible(otherOptions, true);
  checkElementDisplayed(fitToPageContainer, true);
  if (rasterizeEnabled)
    checkElementDisplayed(rasterizeContainer, false);
  expectTrue(
      fitToPageContainer.querySelector('.checkbox').checked);
  this.expandMoreSettings();
  if (rasterizeEnabled) {
    checkElementDisplayed(rasterizeContainer, true);
    expectFalse(
        rasterizeContainer.querySelector('.checkbox').checked);
  }
  checkSectionVisible($('media-size-settings'), true);
  checkSectionVisible(scalingSettings, true);

  this.waitForAnimationToEnd('other-options-collapsible');
});

// When the source is "PDF", depending on the selected destination printer, we
// show/hide the fit to page option and hide media size selection.
TEST_F('PrintPreviewWebUITest', 'ScalingUnchecksFitToPage', function() {
  this.initialSettings_.isDocumentModifiable_ = false;
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  var scalingSettings = $('scaling-settings');

  checkSectionVisible(otherOptions, true);
  var fitToPageContainer =
      otherOptions.querySelector('#fit-to-page-container');
  checkElementDisplayed(fitToPageContainer, true);
  expectTrue(
      fitToPageContainer.querySelector('.checkbox').checked);
  this.expandMoreSettings();
  checkSectionVisible($('media-size-settings'), true);
  checkSectionVisible(scalingSettings, true);

  //Change scaling input
  var scalingInput = scalingSettings.querySelector('.user-value');
  expectEquals(scalingInput.value, '100');
  scalingInput.stepUp(5);
  expectEquals(scalingInput.value, '105');

  // Trigger the event
  var enter = document.createEvent('Event');
  enter.initEvent('keydown');
  enter.keyCode = 'Enter';
  scalingInput.dispatchEvent(enter);
  expectFalse(
      fitToPageContainer.querySelector('.checkbox').checked);

  this.waitForAnimationToEnd('other-options-collapsible');
});

// When the number of copies print preset is set for source 'PDF', we update
// the copies value if capability is supported by printer.
TEST_F('PrintPreviewWebUITest', 'CheckNumCopiesPrintPreset', function() {
  this.initialSettings_.isDocumentModifiable_ = false;
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  // Indicate that the number of copies print preset is set for source PDF.
  var printPresetOptions = {
    disableScaling: true,
    copies: 2
  };
  var printPresetOptionsEvent = new Event(
      print_preview.NativeLayer.EventType.PRINT_PRESET_OPTIONS);
  printPresetOptionsEvent.optionsFromDocument = printPresetOptions;
  this.nativeLayer_.dispatchEvent(printPresetOptionsEvent);

  checkSectionVisible($('copies-settings'), true);
  expectEquals(
      printPresetOptions.copies,
      parseInt($('copies-settings').querySelector('.user-value').value));

  this.waitForAnimationToEnd('other-options-collapsible');
});

// When the duplex print preset is set for source 'PDF', we update the
// duplex setting if capability is supported by printer.
TEST_F('PrintPreviewWebUITest', 'CheckDuplexPrintPreset', function() {
  this.initialSettings_.isDocumentModifiable_ = false;
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  // Indicate that the duplex print preset is set to "long edge" for source PDF.
  var printPresetOptions = {
    duplex: 1
  };
  var printPresetOptionsEvent = new Event(
      print_preview.NativeLayer.EventType.PRINT_PRESET_OPTIONS);
  printPresetOptionsEvent.optionsFromDocument = printPresetOptions;
  this.nativeLayer_.dispatchEvent(printPresetOptionsEvent);

  var otherOptions = $('other-options-settings');
  checkSectionVisible(otherOptions, true);
  var duplexContainer = otherOptions.querySelector('#duplex-container');
  checkElementDisplayed(duplexContainer, true);
  expectTrue(duplexContainer.querySelector('.checkbox').checked);

  this.waitForAnimationToEnd('other-options-collapsible');
});

// Make sure that custom margins controls are properly set up.
TEST_F('PrintPreviewWebUITest', 'CustomMarginsControlsCheck', function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);

  ['left', 'top', 'right', 'bottom'].forEach(function(margin) {
    var control = $('preview-area').querySelector('.margin-control-' + margin);
    assertNotEquals(null, control);
    var input = control.querySelector('.margin-control-textbox');
    assertTrue(input.hasAttribute('aria-label'));
    assertNotEquals('undefined', input.getAttribute('aria-label'));
  });
  this.waitForAnimationToEnd('more-settings');
});

// Page layout has zero margins. Hide header and footer option.
TEST_F('PrintPreviewWebUITest', 'PageLayoutHasNoMarginsHideHeaderFooter',
    function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  var headerFooter = otherOptions.querySelector('#header-footer-container');

  // Check that options are collapsed (section is visible, because duplex is
  // available).
  checkSectionVisible(otherOptions, true);
  checkElementDisplayed(headerFooter, false);

  this.expandMoreSettings();

  checkElementDisplayed(headerFooter, true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
      new print_preview.Margins(0, 0, 0, 0));

  checkElementDisplayed(headerFooter, false);

  this.waitForAnimationToEnd('more-settings');
});

// Page layout has half-inch margins. Show header and footer option.
TEST_F('PrintPreviewWebUITest', 'PageLayoutHasMarginsShowHeaderFooter',
    function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  var headerFooter = otherOptions.querySelector('#header-footer-container');

  // Check that options are collapsed (section is visible, because duplex is
  // available).
  checkSectionVisible(otherOptions, true);
  checkElementDisplayed(headerFooter, false);

  this.expandMoreSettings();

  checkElementDisplayed(headerFooter, true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
      new print_preview.Margins(36, 36, 36, 36));

  checkElementDisplayed(headerFooter, true);

  this.waitForAnimationToEnd('more-settings');
});

// Page layout has zero top and bottom margins. Hide header and footer option.
TEST_F('PrintPreviewWebUITest',
       'ZeroTopAndBottomMarginsHideHeaderFooter',
       function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  var headerFooter = otherOptions.querySelector('#header-footer-container');

  // Check that options are collapsed (section is visible, because duplex is
  // available).
  checkSectionVisible(otherOptions, true);
  checkElementDisplayed(headerFooter, false);

  this.expandMoreSettings();

  checkElementDisplayed(headerFooter, true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
      new print_preview.Margins(0, 36, 0, 36));

  checkElementDisplayed(headerFooter, false);

  this.waitForAnimationToEnd('more-settings');
});

// Page layout has zero top and half-inch bottom margin. Show header and footer
// option.
TEST_F('PrintPreviewWebUITest',
       'ZeroTopAndNonZeroBottomMarginShowHeaderFooter',
       function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  var headerFooter = otherOptions.querySelector('#header-footer-container');

  // Check that options are collapsed (section is visible, because duplex is
  // available).
  checkSectionVisible(otherOptions, true);
  checkElementDisplayed(headerFooter, false);

  this.expandMoreSettings();

  checkElementDisplayed(headerFooter, true);

  printPreview.printTicketStore_.marginsType.updateValue(
      print_preview.ticket_items.MarginsType.Value.CUSTOM);
  printPreview.printTicketStore_.customMargins.updateValue(
      new print_preview.Margins(0, 36, 36, 36));

  checkElementDisplayed(headerFooter, true);

  this.waitForAnimationToEnd('more-settings');
});

// Test that the color settings, one option, standard monochrome.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsMonochrome', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  // Only one option, standard monochrome.
  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_MONOCHROME"}
    ]
  };
  this.setCapabilities(device);

  checkSectionVisible($('color-settings'), false);

  this.waitForAnimationToEnd('more-settings');
});

// Test that the color settings, one option, custom monochrome.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsCustomMonochrome',
    function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  // Only one option, standard monochrome.
  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "CUSTOM_MONOCHROME", "vendor_id": "42"}
    ]
  };
  this.setCapabilities(device);

  checkSectionVisible($('color-settings'), false);

  this.waitForAnimationToEnd('more-settings');
});

// Test that the color settings, one option, standard color.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsColor', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_COLOR"}
    ]
  };
  this.setCapabilities(device);

  checkSectionVisible($('color-settings'), false);

  this.waitForAnimationToEnd('more-settings');
});

// Test that the color settings, one option, custom color.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsCustomColor', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "CUSTOM_COLOR", "vendor_id": "42"}
    ]
  };
  this.setCapabilities(device);

  checkSectionVisible($('color-settings'), false);

  this.waitForAnimationToEnd('more-settings');
});

// Test that the color settings, two options, both standard, defaults to color.
TEST_F('PrintPreviewWebUITest', 'TestColorSettingsBothStandardDefaultColor',
    function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"type": "STANDARD_MONOCHROME"},
      {"is_default": true, "type": "STANDARD_COLOR"}
    ]
  };
  this.setCapabilities(device);

  checkSectionVisible($('color-settings'), true);
  expectEquals(
      'color',
      $('color-settings').querySelector('.color-settings-select').value);

  this.waitForAnimationToEnd('more-settings');
});

// Test that the color settings, two options, both standard, defaults to
// monochrome.
TEST_F('PrintPreviewWebUITest',
    'TestColorSettingsBothStandardDefaultMonochrome', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_MONOCHROME"},
      {"type": "STANDARD_COLOR"}
    ]
  };
  this.setCapabilities(device);

  checkSectionVisible($('color-settings'), true);
  expectEquals(
      'bw', $('color-settings').querySelector('.color-settings-select').value);

  this.waitForAnimationToEnd('more-settings');
});

// Test that the color settings, two options, both custom, defaults to color.
TEST_F('PrintPreviewWebUITest',
    'TestColorSettingsBothCustomDefaultColor', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.color = {
    "option": [
      {"type": "CUSTOM_MONOCHROME", "vendor_id": "42"},
      {"is_default": true, "type": "CUSTOM_COLOR", "vendor_id": "43"}
    ]
  };
  this.setCapabilities(device);

  checkSectionVisible($('color-settings'), true);
  expectEquals(
      'color',
      $('color-settings').querySelector('.color-settings-select').value);

  this.waitForAnimationToEnd('more-settings');
});

// Test to verify that duplex settings are set according to the printer
// capabilities.
TEST_F('PrintPreviewWebUITest', 'TestDuplexSettingsTrue', function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var otherOptions = $('other-options-settings');
  checkSectionVisible(otherOptions, true);
  duplexContainer = otherOptions.querySelector('#duplex-container');
  expectFalse(duplexContainer.hidden);
  expectFalse(duplexContainer.querySelector('.checkbox').checked);

  this.waitForAnimationToEnd('more-settings');
});

// Test to verify that duplex settings are set according to the printer
// capabilities.
TEST_F('PrintPreviewWebUITest', 'TestDuplexSettingsFalse', function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  var device = getCddTemplate("FooDevice");
  delete device.capabilities.printer.duplex;
  this.setCapabilities(device);

  // Check that it is collapsed.
  var otherOptions = $('other-options-settings');
  checkSectionVisible(otherOptions, false);

  this.expandMoreSettings();

  // Now it should be visible.
  checkSectionVisible(otherOptions, true);
  expectTrue(otherOptions.querySelector('#duplex-container').hidden);

  this.waitForAnimationToEnd('more-settings');
});

// Test that changing the selected printer updates the preview.
TEST_F('PrintPreviewWebUITest', 'TestPrinterChangeUpdatesPreview', function() {
  this.setInitialSettings();
  this.setLocalDestinations();
  this.setCapabilities(getCddTemplate("FooDevice"));

  var previewGenerator = mock(print_preview.PreviewGenerator);
  printPreview.previewArea_.previewGenerator_ = previewGenerator.proxy();

  // The number of settings that can change due to a change in the destination
  // that will therefore dispatch ticket item change events.
  previewGenerator.expects(exactly(7)).requestPreview();

  var barDestination;
  var destinations = printPreview.destinationStore_.destinations();
  for (var destination, i = 0; destination = destinations[i]; i++) {
    if (destination.id == 'BarDevice') {
      barDestination = destination;
      break;
    }
  }

  printPreview.destinationStore_.selectDestination(barDestination);

  var device = getCddTemplate("BarDevice");
  device.capabilities.printer.color = {
    "option": [
      {"is_default": true, "type": "STANDARD_MONOCHROME"}
    ]
  };
  this.setCapabilities(device);

  this.waitForAnimationToEnd('more-settings');
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

  testDone();
});

// Test custom localized paper names.
TEST_F('PrintPreviewWebUITest', 'TestCustomPaperNames', function() {
  this.setInitialSettings();
  this.setLocalDestinations();

  var customLocalizedMediaName = 'Vendor defined localized media name';
  var customMediaName = 'Vendor defined media name';

  var device = getCddTemplate("FooDevice");
  device.capabilities.printer.media_size = {
    option: [
      { name: 'CUSTOM',
        width_microns: 15900,
        height_microns: 79400,
        is_default: true,
        custom_display_name_localized: [
          { locale: navigator.language,
            value: customLocalizedMediaName
          }
        ]
      },
      { name: 'CUSTOM',
        width_microns: 15900,
        height_microns: 79400,
        custom_display_name: customMediaName
      }
    ]
  };

  this.setCapabilities(device);

  this.expandMoreSettings();

  checkSectionVisible($('media-size-settings'), true);
  var mediaSelect = $('media-size-settings').querySelector('.settings-select');
  // Check the default media item.
  expectEquals(
      customLocalizedMediaName,
      mediaSelect.options[mediaSelect.selectedIndex].text);
  // Check the other media item.
  expectEquals(
      customMediaName,
      mediaSelect.options[mediaSelect.selectedIndex == 0 ? 1 : 0].text);

  this.waitForAnimationToEnd('more-settings');
});

function getCddTemplateWithAdvancedSettings(printerId) {
  return {
    printerId: printerId,
    capabilities: {
      version: '1.0',
      printer: {
        supported_content_type: [{content_type: 'application/pdf'}],
        vendor_capability:
        [
          {display_name: 'Print Area',
            id: 'Print Area',
            type: 'SELECT',
            select_cap: {
              option: [
                {display_name: 'A4', value: 4, is_default: true},
                {display_name: 'A6', value: 6},
                {display_name: 'A7', value: 7}
              ]
            }
          }
        ],
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
        },
      }
    }
  };
}

// Simulates a click of the advanced options settings button to bring up the
// advanced settings overlay.
function openAdvancedSettings() {
  // Check for button and click to view advanced settings section.
  var advancedOptionsSettingsButton =
      $('advanced-options-settings').
      querySelector('.advanced-options-settings-button');
  checkElementDisplayed(advancedOptionsSettingsButton, true);
  // Button is disabled during testing due to test version of
  // testPluginCompatibility() being set to always return false. Enable button
  // to send click event.
  advancedOptionsSettingsButton.disabled = false;
  advancedOptionsSettingsButton.click();
}

// Test advanced settings with 1 capability (should not display settings search
// box).
TEST_F('PrintPreviewWebUITest', 'TestAdvancedSettings1Option', function() {
  var device = getCddTemplateWithAdvancedSettings("FooDevice");
  this.setupAdvancedSettingsTest(device);

  // Open the advanced settings overlay.
  openAdvancedSettings();

  // Check that advanced settings close button is now visible,
  // but not the search box (only 1 capability).
  var advancedSettingsCloseButton = $('advanced-settings').
        querySelector('.close-button');
  checkElementDisplayed(advancedSettingsCloseButton, true);
  checkElementDisplayed($('advanced-settings').
       querySelector('.search-box-area'), false);

  this.waitForAnimationToEnd('more-settings');
});


// Test advanced settings with 2 capabilities (should have settings search box).
TEST_F('PrintPreviewWebUITest', 'TestAdvancedSettings2Options', function() {
  var device = getCddTemplateWithAdvancedSettings("FooDevice");
   // Add new capability.
  device.capabilities.printer.vendor_capability.push({
      display_name: 'Paper Type',
      id: 'Paper Type',
      type: 'SELECT',
      select_cap: {
          option: [
              {display_name: 'Standard', value: 0, is_default: true},
              {display_name: 'Recycled', value: 1},
              {display_name: 'Special', value: 2}
          ]
      }
  });
  this.setupAdvancedSettingsTest(device);

  // Open the advanced settings overlay.
  openAdvancedSettings();

  // Check advanced settings is visible and that the search box now
  // appears.
  var advancedSettingsCloseButton = $('advanced-settings').
      querySelector('.close-button');
  checkElementDisplayed(advancedSettingsCloseButton, true);
  checkElementDisplayed($('advanced-settings').
      querySelector('.search-box-area'), true);

  this.waitForAnimationToEnd('more-settings');
});

// Test that initialization with saved destination only issues one call
// to startPreview.
TEST_F('PrintPreviewWebUITest', 'TestInitIssuesOneRequest', function() {
  // Load in a bunch of recent destinations with non null capabilities.
  var origin = cr.isChromeOS ? 'chrome_os' : 'local';
  var initSettings = {
    version: 2,
    recentDestinations: [1, 2, 3].map(function(i) {
      return {
        id: 'ID' + i, origin: origin, account: '',
        capabilities: getCddTemplate('ID' + i), name: '',
        extensionId: '', extensionName: ''
      };
    }),
  };
  this.initialSettings_.serializedAppStateStr_ = JSON.stringify(initSettings);
  this.setCapabilities(getCddTemplate('ID1'));
  this.setCapabilities(getCddTemplate('ID2'));
  this.setCapabilities(getCddTemplate('ID3'));

  // Use a real preview generator.
  printPreview.previewArea_.previewGenerator_ =
      new print_preview.PreviewGenerator(printPreview.destinationStore_,
        printPreview.printTicketStore_, this.nativeLayer_,
        printPreview.documentInfo_);

  // Preview generator starts out with inFlightRequestId_ == -1. The id
  // increments by 1 for each startGetPreview call it makes. It should only
  // make one such call during initialization or there will be a race; see
  // crbug.com/666595
  expectEquals(printPreview.previewArea_.previewGenerator_.inFlightRequestId_,
    -1);
  this.setInitialSettings();
  expectEquals(printPreview.previewArea_.previewGenerator_.inFlightRequestId_,
    0);
  testDone();
});
