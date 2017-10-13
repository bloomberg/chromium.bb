// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview_test', function() {
  /**
   * Index of the "Save as PDF" printer.
   * @type {number}
   * @const
   */
  var PDF_INDEX = 0;

  /**
   * Index of the Foo printer.
   * @type {number}
   * @const
   */
  var FOO_INDEX = 1;

  /**
   * Index of the Bar printer.
   * @type {number}
   * @const
   */
  var BAR_INDEX = 2;

  var printPreview = null;
  var nativeLayer = null;
  var initialSettings = null;
  var localDestinationInfos = null;
  var previewArea = null;

  /**
   * Initialize print preview with the initial settings currently stored in
   * |initialSettings|.
   */
  function setInitialSettings() {
    nativeLayer.setInitialSettings(initialSettings);
    printPreview.initialize();
  }

  /**
   * Sets settings and destinations and local destination that is the system
   * default.
   * @param {print_preview.PrinterCapabilitiesResponse=} opt_device The
   *     response to use for printer capabilities when the printer represented
   *     by |opt_device| is loaded. To avoid crashing when initialize() is
   *     called, |opt_device| should represent the printer that will be selected
   *     when print preview is first opened, i.e. the system default
   *     destination, or the most recently used destination or destination
   *     selected by the rules string if these parameters are defined in
   *     initialSettings.serializedAppStateStr_.
   *     If |opt_device| is not provided, a default device with ID 'FooDevice'
   *     will be used.
   * @return {!Promise<print_preview.PrinterCapabilitiesResponse>} a
   *     promise that will resolve when getPrinterCapabilities has been
   *     called for the device (either default or provided).
   */
  function setupSettingsAndDestinationsWithCapabilities(opt_device) {
    nativeLayer.setInitialSettings(initialSettings);
    nativeLayer.setLocalDestinations(localDestinationInfos);
    opt_device = opt_device || getCddTemplate('FooDevice', 'FooName');
    nativeLayer.setLocalDestinationCapabilities(opt_device);

    printPreview.initialize();
    return nativeLayer.whenCalled('getInitialSettings').then(function() {
      printPreview.destinationStore_.startLoadDestinations(
          print_preview.PrinterType.LOCAL_PRINTER);
      return Promise.all([
        nativeLayer.whenCalled('getPrinters'),
        nativeLayer.whenCalled('getPrinterCapabilities')
      ]);
    });
  }

  /**
   * Verify that |section| visibility matches |visible|.
   * @param {HTMLDivElement} section The section to check.
   * @param {boolean} visible The expected state of visibility.
   */
  function checkSectionVisible(section, visible) {
    assertNotEquals(null, section);
    expectEquals(
        visible,
        section.classList.contains('visible'), 'section=' + section.id);
  }

  /**
   * @param {?HTMLElement} el
   * @param {boolean} isDisplayed
   */
  function checkElementDisplayed(el, isDisplayed) {
    assertNotEquals(null, el);
    expectEquals(isDisplayed,
                 !el.hidden,
                 'element="' + el.id + '" of class "' + el.classList + '"');
  }

  /**
   * @param {string} printerId
   * @param {string=} opt_printerName Defaults to an empty string.
   * @return {!print_preview.PrinterCapabilitiesResponse}
   */
  function getCddTemplate(printerId, opt_printerName) {
    return {
      printer: {
        deviceName: printerId,
        printerName: opt_printerName || '',
      },
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

  /**
   * @return {!print_preview.PrinterCapabilitiesResponse} The capabilities of
   *     the Save as PDF destination.
   */
  function getPdfPrinter() {
    return {
      printer: {
        deviceName: 'Save as PDF',
      },
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
  }

  /**
   * Get the default media size for |device|.
   * @param {!print_preview.PrinterCapabilitiesResponse} device
   * @return {{width_microns: number,
   *           height_microns: number}} The width and height of the default
   *     media.
   */
  function getDefaultMediaSize(device) {
    var size = device.capabilities.printer.media_size.option.find(
        function(opt) { return opt.is_default; });
    return { width_microns: size.width_microns,
             height_microns: size.height_microns };
  }

  /**
   * Get the default page orientation for |device|.
   * @param {!print_preview.PrinterCapabilitiesResponse} device
   * @return {string} The default orientation.
   */
  function getDefaultOrientation(device) {
    return device.capabilities.printer.page_orientation.option.find(
        function(opt) { return opt.is_default; }).type;
  }


  /**
   * @param {string} printerId
   * @return {!Object}
   */
  function getCddTemplateWithAdvancedSettings(printerId) {
    var template = getCddTemplate(printerId);
    template.capabilities.printer.vendor_capability = [{
      display_name: 'Print Area',
      id: 'Print Area',
      type: 'SELECT',
      select_cap: {
        option: [
          {display_name: 'A4', value: 4, is_default: true},
          {display_name: 'A6', value: 6},
          {display_name: 'A7', value: 7},
        ],
      },
    }];
    return template;
  }

  /**
   * Even though animation duration and delay is set to zero, it is necessary to
   * wait until the animation has finished.
   * @return {!Promise} A promise firing when the animation is done.
   */
  function whenAnimationDone(elementId) {
    return new Promise(function(resolve) {
      // Add a listener for the animation end event.
      var element = $(elementId);
      element.addEventListener('animationend', function f(e) {
        element.removeEventListener('animationend', f);
        resolve();
      });
    });
  }

  /**
   * Expand the 'More Settings' div to expose all options.
   */
  function expandMoreSettings() {
    var moreSettings = $('more-settings');
    checkSectionVisible(moreSettings, true);
    moreSettings.click();
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

  /**
   * Repeated setup steps for the advanced settings tests.
   * Sets capabilities, and verifies advanced options section is visible
   * after expanding more settings. Then opens the advanced settings overlay
   * and verifies it is displayed.
   */
  function startAdvancedSettingsTest(device) {
    expandMoreSettings();

    // Check that the advanced options settings section is visible.
    checkSectionVisible($('advanced-options-settings'), true);

    // Open the advanced settings overlay.
    openAdvancedSettings();

    // Check advanced settings overlay is visible by checking that the close
    // button is displayed.
    var advancedSettingsCloseButton = $('advanced-settings').
        querySelector('.close-button');
    checkElementDisplayed(advancedSettingsCloseButton, true);
  }

  /** @return {boolean} */
  function isPrintAsImageEnabled() {
    // Should be enabled by default on non Windows/Mac.
    return (!cr.isWindows && !cr.isMac &&
            loadTimeData.getBoolean('printPdfAsImageEnabled'));
  }

  suite('PrintPreview', function() {
    suiteSetup(function() {
      function CloudPrintInterfaceStub() {
        cr.EventTarget.call(this);
      }
      CloudPrintInterfaceStub.prototype = {
        __proto__: cr.EventTarget.prototype,
        search: function(isRecent) {}
      };
      var oldCpInterfaceEventType = cloudprint.CloudPrintInterfaceEventType;
      cloudprint.CloudPrintInterface = CloudPrintInterfaceStub;
      cloudprint.CloudPrintInterfaceEventType = oldCpInterfaceEventType;

      print_preview.PreviewArea.prototype.checkPluginCompatibility_ =
          function() {
        return true;
      };
    });

    setup(function() {
      initialSettings = {
        isInKioskAutoPrintMode: false,
        isInAppKioskMode: false,
        thousandsDelimeter: ',',
        decimalDelimeter: '.',
        unitType: 1,
        previewModifiable: true,
        documentTitle: 'title',
        documentHasSelection: true,
        shouldPrintSelectionOnly: false,
        printerName: 'FooDevice',
        serializedAppStateStr: null,
        serializedDefaultDestinationSelectionRulesStr: null
      };

      localDestinationInfos = [
        { printerName: 'FooName', deviceName: 'FooDevice' },
        { printerName: 'BarName', deviceName: 'BarDevice' },
      ];

      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      printPreview = new print_preview.PrintPreview();
      previewArea = printPreview.getPreviewArea();
      previewArea.plugin_ = new print_preview.PDFPluginStub(previewArea);
      previewArea.plugin_.setLoadCallback(
          previewArea.onPluginLoad_.bind(previewArea));
    });

    // Test some basic assumptions about the print preview WebUI.
    test('PrinterList', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var recentList =
            $('destination-search').querySelector('.recent-list ul');
        var localList =
            $('destination-search').querySelector('.local-list ul');
        assertNotEquals(null, recentList);
        assertEquals(1, recentList.childNodes.length);
        assertEquals('FooName',
                     recentList.childNodes.item(0).querySelector(
                         '.destination-list-item-name').textContent);
        assertNotEquals(null, localList);
        assertEquals(3, localList.childNodes.length);
        assertEquals(
            'Save as PDF',
            localList.childNodes.item(PDF_INDEX).
            querySelector('.destination-list-item-name').textContent);
        assertEquals(
            'FooName',
            localList.childNodes.item(FOO_INDEX).
            querySelector('.destination-list-item-name').textContent);
        assertEquals(
            'BarName',
            localList.childNodes.item(BAR_INDEX).
            querySelector('.destination-list-item-name').textContent);
      });
    });

    // Test that the printer list is structured correctly after calling
    // addCloudPrinters with an empty list.
    test('PrinterListCloudEmpty', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        cr.webUIListenerCallback('use-cloud-print', 'cloudprint url', false);
        var searchDoneEvent =
            new Event(cloudprint.CloudPrintInterfaceEventType.SEARCH_DONE);
        searchDoneEvent.printers = [];
        searchDoneEvent.isRecent = true;
        searchDoneEvent.email = 'foo@chromium.org';
        printPreview.cloudPrintInterface_.dispatchEvent(searchDoneEvent);

        var recentList =
            $('destination-search').querySelector('.recent-list ul');
        var localList =
            $('destination-search').querySelector('.local-list ul');
        var cloudList =
            $('destination-search').querySelector('.cloud-list ul');

        assertNotEquals(null, recentList);
        assertEquals(1, recentList.childNodes.length);
        assertEquals('FooName',
                     recentList.childNodes.item(0).
                         querySelector('.destination-list-item-name').
                         textContent);

        assertNotEquals(null, localList);
        assertEquals(3, localList.childNodes.length);
        assertEquals('Save as PDF',
                     localList.childNodes.item(PDF_INDEX).
                         querySelector('.destination-list-item-name').
                         textContent);
        assertEquals('FooName',
                     localList.childNodes.
                         item(FOO_INDEX).
                         querySelector('.destination-list-item-name').
                         textContent);
        assertEquals('BarName',
                     localList.childNodes.
                         item(BAR_INDEX).
                         querySelector('.destination-list-item-name').
                         textContent);

        assertNotEquals(null, cloudList);
        assertEquals(0, cloudList.childNodes.length);
      });
    });

    // Test restore settings with one destination.
    test('RestoreLocalDestination', function() {
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [
          {
            id: 'ID',
            origin: cr.isChromeOS ? 'chrome_os' : 'local',
            account: '',
            capabilities: 0,
            displayName: '',
            extensionId: '',
            extensionName: '',
          },
        ],
      });
      nativeLayer.setLocalDestinationCapabilities(getCddTemplate('ID'));
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings');
    });

    test('RestoreMultipleDestinations', function() {
      var origin = cr.isChromeOS ? 'chrome_os' : 'local';

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [
          {
            id: 'ID1',
            origin: origin,
            account: '',
            capabilities: 0,
            displayName: 'One',
            extensionId: '',
            extensionName: '',
          }, {
            id: 'ID2',
            origin: origin,
            account: '',
            capabilities: 0,
            displayName: 'Two',
            extensionId: '',
            extensionName: '',
          }, {
            id: 'ID3',
            origin: origin,
            account: '',
            capabilities: 0,
            displayName: 'Three',
            extensionId: '',
            extensionName: '',
          },
        ],
      });
      // Set all three of these destinations in the local destination infos
      // (represents currently available printers), plus an extra destination.
      localDestinationInfos = [
        { printerName: 'One', deviceName: 'ID1' },
        { printerName: 'Two', deviceName: 'ID2' },
        { printerName: 'Three', deviceName: 'ID3' },
        { printerName: 'Four', deviceName: 'ID4' }
      ];

      // Set up capabilities for ID1. This should be the device that should hav
      // its capabilities fetched, since it is the most recent. If another
      // device is selected the native layer will reject the callback.
      var device = getCddTemplate('ID1', 'One');

      return setupSettingsAndDestinationsWithCapabilities(device).then(
          function() {
            // The most recently used destination should be the currently
            // selected one. This is ID1.
            assertEquals(
                'ID1', printPreview.destinationStore_.selectedDestination.id);

            // Look through the destinations. ID1, ID2, and ID3 should all be
            // recent.
            var destinations = printPreview.destinationStore_.destinations_;
            var idsFound = [];

            for (var i = 0; i < destinations.length; i++) {
              if (!destinations[i])
                continue;
              if (destinations[i].isRecent)
                idsFound.push(destinations[i].id);
            }

            // Make sure there were 3 recent destinations and that they are the
            // correct IDs.
            assertEquals(3, idsFound.length);
            assertNotEquals(-1, idsFound.indexOf('ID1'));
            assertNotEquals(-1, idsFound.indexOf('ID2'));
            assertNotEquals(-1, idsFound.indexOf('ID3'));
          });
    });

    test('DefaultDestinationSelectionRules', function() {
      // It also makes sure these rules do override system default destination.
      initialSettings.serializedDefaultDestinationSelectionRulesStr =
          JSON.stringify({namePattern: '.*Bar.*'});
      return setupSettingsAndDestinationsWithCapabilities(
          getCddTemplate('BarDevice', 'BarName')).then(function() {
            assertEquals('BarDevice',
                         printPreview.destinationStore_.selectedDestination.id);
          });
    });

    test('SystemDialogLinkIsHiddenInAppKioskMode', function() {
      if (!cr.isChromeOS)
        initialSettings.isInAppKioskMode = true;
      nativeLayer.setLocalDestinationCapabilities(getCddTemplate('FooDevice'));
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            if (cr.isChromeOS)
              assertEquals(null, $('system-dialog-link'));
            else
              checkElementDisplayed($('system-dialog-link'), false);
          });
    });

    test('SectionsDisabled', function() {
      checkSectionVisible($('layout-settings'), false);
      checkSectionVisible($('color-settings'), false);
      checkSectionVisible($('copies-settings'), false);
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        option: [{is_default: true, type: 'STANDARD_COLOR'}]
      };
      delete device.capabilities.printer.copies;

      return setupSettingsAndDestinationsWithCapabilities(device)
        .then(function() {
          checkSectionVisible($('layout-settings'), true);
          checkSectionVisible($('color-settings'), false);
          checkSectionVisible($('copies-settings'), false);

          return whenAnimationDone('other-options-collapsible');
        });
    });

    // When the source is 'PDF' and 'Save as PDF' option is selected, we hide
    // the fit to page option.
    test('PrintToPDFSelectedCapabilities', function() {
      // Setup initial settings
      initialSettings.previewModifiable = false;
      initialSettings.printerName = 'Save as PDF';

      // Set PDF printer
      nativeLayer.setLocalDestinationCapabilities(getPdfPrinter());

      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        return nativeLayer.whenCalled('getPrinterCapabilities');
      }).then(function() {
        var otherOptions = $('other-options-settings');
        var scalingSettings = $('scaling-settings');
        // If rasterization is an option, other options should be visible.
        // If not, there should be no available other options.
        checkSectionVisible(otherOptions, isPrintAsImageEnabled());
        if (isPrintAsImageEnabled()) {
          checkElementDisplayed(
              otherOptions.querySelector('#rasterize-container'), true);
        }
        checkSectionVisible($('media-size-settings'), false);
        checkSectionVisible(scalingSettings, false);
        checkElementDisplayed(
            scalingSettings.querySelector('#fit-to-page-container'), false);
      });
    });

    // When the source is 'HTML', we always hide the fit to page option and show
    // media size option.
    test('SourceIsHTMLCapabilities', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var otherOptions = $('other-options-settings');
        var rasterize;
        if (isPrintAsImageEnabled())
          rasterize = otherOptions.querySelector('#rasterize-container');
        var mediaSize = $('media-size-settings');
        var scalingSettings = $('scaling-settings');
        var fitToPage = scalingSettings.querySelector('#fit-to-page-container');

        // Check that options are collapsed (section is visible, because
        // duplex is available).
        checkSectionVisible(otherOptions, true);
        checkElementDisplayed(fitToPage, false);
        if (isPrintAsImageEnabled())
          checkElementDisplayed(rasterize, false);
        checkSectionVisible(mediaSize, false);
        checkSectionVisible(scalingSettings, false);

        expandMoreSettings();

        checkElementDisplayed(fitToPage, false);
        if (isPrintAsImageEnabled())
          checkElementDisplayed(rasterize, false);
        checkSectionVisible(mediaSize, true);
        checkSectionVisible(scalingSettings, true);

        return whenAnimationDone('more-settings');
      });
    });

    // When the source is 'PDF', depending on the selected destination printer,
    // we show/hide the fit to page option and hide media size selection.
    test('SourceIsPDFCapabilities', function() {
      initialSettings.previewModifiable = false;
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var otherOptions = $('other-options-settings');
        var scalingSettings = $('scaling-settings');
        var fitToPageContainer =
            scalingSettings.querySelector('#fit-to-page-container');
        var rasterizeContainer;
        if (isPrintAsImageEnabled()) {
          rasterizeContainer =
            otherOptions.querySelector('#rasterize-container');
        }

        checkSectionVisible(otherOptions, true);
        checkElementDisplayed(fitToPageContainer, true);
        if (isPrintAsImageEnabled())
          checkElementDisplayed(rasterizeContainer, false);
        expectTrue(
            fitToPageContainer.querySelector('.checkbox').checked);
        expandMoreSettings();
        if (isPrintAsImageEnabled()) {
          checkElementDisplayed(rasterizeContainer, true);
          expectFalse(
              rasterizeContainer.querySelector('.checkbox').checked);
        }
        checkSectionVisible($('media-size-settings'), true);
        checkSectionVisible(scalingSettings, true);

        return whenAnimationDone('other-options-collapsible');
      });
    });

    // When the source is 'PDF', depending on the selected destination printer,
    // we show/hide the fit to page option and hide media size selection.
    test('ScalingUnchecksFitToPage', function() {
      initialSettings.previewModifiable = false;
      // Wait for preview to load.
      return Promise.all([setupSettingsAndDestinationsWithCapabilities(),
                          nativeLayer.whenCalled('getPreview')]).then(
        function(args) {
          var scalingSettings = $('scaling-settings');
          checkSectionVisible(scalingSettings, true);
          var fitToPageContainer =
              scalingSettings.querySelector('#fit-to-page-container');
          checkElementDisplayed(fitToPageContainer, true);
          expectTrue(args[1].printTicketStore.fitToPage.getValue());
          expectEquals('100', args[1].printTicketStore.scaling.getValue());
          expectTrue(fitToPageContainer.querySelector('.checkbox').checked);
          expandMoreSettings();
          checkSectionVisible($('media-size-settings'), true);
          checkSectionVisible(scalingSettings, true);
          nativeLayer.resetResolver('getPreview');

          // Change scaling input
          var scalingInput = scalingSettings.querySelector('.user-value');
          expectEquals('100', scalingInput.value);
          scalingInput.stepUp(5);
          expectEquals('105', scalingInput.value);

          // Trigger the event
          var enterEvent = document.createEvent('Event');
          enterEvent.initEvent('keydown');
          enterEvent.keyCode = 'Enter';
          scalingInput.dispatchEvent(enterEvent);

          // Wait for the preview to refresh and verify print ticket and
          // display.
          return nativeLayer.whenCalled('getPreview').then(function(args) {
            expectFalse(args.printTicketStore.fitToPage.getValue());
            expectEquals('105', args.printTicketStore.scaling.getValue());
            expectFalse(fitToPageContainer.querySelector('.checkbox').checked);
            return whenAnimationDone('more-settings');
          });
        });
    });

    // When the number of copies print preset is set for source 'PDF', we update
    // the copies value if capability is supported by printer.
    test('CheckNumCopiesPrintPreset', function() {
      initialSettings.previewModifiable = false;
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        // Indicate that the number of copies print preset is set for source
        // PDF.
        var copies = 2;
        cr.webUIListenerCallback('print-preset-options', true, copies);
        checkSectionVisible($('copies-settings'), true);
        expectEquals(
            copies,
            parseInt($('copies-settings').querySelector('.user-value').value));

        return whenAnimationDone('other-options-collapsible');
      });
    });

    // When the duplex print preset is set for source 'PDF', we update the
    // duplex setting if capability is supported by printer.
    test('CheckDuplexPrintPreset', function() {
      initialSettings.previewModifiable = false;
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        // Indicate that the duplex print preset is set to 'long edge' for
        // source PDF.
        cr.webUIListenerCallback('print-preset-options', false, 1, 1);
        var otherOptions = $('other-options-settings');
        checkSectionVisible(otherOptions, true);
        var duplexContainer =
            otherOptions.querySelector('#duplex-container');
        checkElementDisplayed(duplexContainer, true);
        expectTrue(duplexContainer.querySelector('.checkbox').checked);

        return whenAnimationDone('other-options-collapsible');
      });
    });

    // Make sure that custom margins controls are properly set up.
    test('CustomMarginsControlsCheck', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        printPreview.printTicketStore_.marginsType.updateValue(
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);

        ['left', 'top', 'right', 'bottom'].forEach(function(margin) {
          var control =
              $('preview-area').querySelector('.margin-control-' + margin);
          assertNotEquals(null, control);
          var input = control.querySelector('.margin-control-textbox');
          assertTrue(input.hasAttribute('aria-label'));
          assertNotEquals('undefined', input.getAttribute('aria-label'));
        });
        return whenAnimationDone('more-settings');
      });
    });

    // Page layout has zero margins. Hide header and footer option.
    test('PageLayoutHasNoMarginsHideHeaderFooter', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because
        // duplex is available).
        checkSectionVisible(otherOptions, true);
        checkElementDisplayed(headerFooter, false);

        expandMoreSettings();

        checkElementDisplayed(headerFooter, true);

        printPreview.printTicketStore_.marginsType.updateValue(
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);
        printPreview.printTicketStore_.customMargins.updateValue(
            new print_preview.Margins(0, 0, 0, 0));

        checkElementDisplayed(headerFooter, false);

        return whenAnimationDone('more-settings');
      });
    });

    // Page layout has half-inch margins. Show header and footer option.
    test('PageLayoutHasMarginsShowHeaderFooter', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because
        // duplex is available).
        checkSectionVisible(otherOptions, true);
        checkElementDisplayed(headerFooter, false);

        expandMoreSettings();

        checkElementDisplayed(headerFooter, true);

        printPreview.printTicketStore_.marginsType.updateValue(
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);
        printPreview.printTicketStore_.customMargins.updateValue(
            new print_preview.Margins(36, 36, 36, 36));

        checkElementDisplayed(headerFooter, true);

        return whenAnimationDone('more-settings');
      });
    });

    // Page layout has zero top and bottom margins. Hide header and footer
    // option.
    test('ZeroTopAndBottomMarginsHideHeaderFooter', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because
        // duplex is available).
        checkSectionVisible(otherOptions, true);
        checkElementDisplayed(headerFooter, false);

        expandMoreSettings();

        checkElementDisplayed(headerFooter, true);

        printPreview.printTicketStore_.marginsType.updateValue(
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);
        printPreview.printTicketStore_.customMargins.updateValue(
            new print_preview.Margins(0, 36, 0, 36));

        checkElementDisplayed(headerFooter, false);

        return whenAnimationDone('more-settings');
      });
    });

    // Page layout has zero top and half-inch bottom margin. Show header and
    // footer option.
    test('ZeroTopAndNonZeroBottomMarginShowHeaderFooter', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because
        // duplex is available).
        checkSectionVisible(otherOptions, true);
        checkElementDisplayed(headerFooter, false);

        expandMoreSettings();

        checkElementDisplayed(headerFooter, true);

        printPreview.printTicketStore_.marginsType.updateValue(
            print_preview.ticket_items.MarginsTypeValue.CUSTOM);
        printPreview.printTicketStore_.customMargins.updateValue(
            new print_preview.Margins(0, 36, 36, 36));

        checkElementDisplayed(headerFooter, true);

        return whenAnimationDone('more-settings');
      });
    });

    // Check header footer availability with small (label) page size.
    test('SmallPaperSizeHeaderFooter', function() {
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.media_size = {
        'option': [
          {'name': 'SmallLabel', 'width_microns': 38100,
            'height_microns': 12700, 'is_default': false},
          {'name': 'BigLabel', 'width_microns': 50800,
            'height_microns': 76200, 'is_default': true}
        ]
      };
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because
        // duplex is available).
        checkSectionVisible(otherOptions, true);
        checkElementDisplayed(headerFooter, false);

        expandMoreSettings();

        // Big label should have header/footer
        checkElementDisplayed(headerFooter, true);

        // Small label should not
        printPreview.printTicketStore_.mediaSize.updateValue(
            device.capabilities.printer.media_size.option[0]);
        checkElementDisplayed(headerFooter, false);

        // Oriented in landscape, there should be enough space for
        // header/footer.
        printPreview.printTicketStore_.landscape.updateValue(true);
        checkElementDisplayed(headerFooter, true);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, one option, standard monochrome.
    test('ColorSettingsMonochrome', function() {
      // Only one option, standard monochrome.
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        'option': [
          {'is_default': true, 'type': 'STANDARD_MONOCHROME'}
        ]
      };

      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, one option, custom monochrome.
    test('ColorSettingsCustomMonochrome', function() {
      // Only one option, standard monochrome.
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        'option': [
          {'is_default': true, 'type': 'CUSTOM_MONOCHROME',
           'vendor_id': '42'}
        ]
      };

      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, one option, standard color.
    test('ColorSettingsColor', function() {
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        'option': [
          {'is_default': true, 'type': 'STANDARD_COLOR'}
        ]
      };

      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, one option, custom color.
    test('ColorSettingsCustomColor', function() {
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        'option': [
          {'is_default': true, 'type': 'CUSTOM_COLOR', 'vendor_id': '42'}
        ]
      };
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, two options, both standard, defaults to
    // color.
    test('ColorSettingsBothStandardDefaultColor', function() {
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        'option': [
          {'type': 'STANDARD_MONOCHROME'},
          {'is_default': true, 'type': 'STANDARD_COLOR'}
        ]
      };
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        checkSectionVisible($('color-settings'), true);
        expectEquals(
            'color',
            $('color-settings').querySelector(
                '.color-settings-select').value);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, two options, both standard, defaults to
    // monochrome.
    test('ColorSettingsBothStandardDefaultMonochrome', function() {
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        'option': [
          {'is_default': true, 'type': 'STANDARD_MONOCHROME'},
          {'type': 'STANDARD_COLOR'}
        ]
      };
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        checkSectionVisible($('color-settings'), true);
        expectEquals(
            'bw',
            $('color-settings').querySelector(
                '.color-settings-select').value);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, two options, both custom, defaults to
    // color.
    test('ColorSettingsBothCustomDefaultColor', function() {
      var device = getCddTemplate('FooDevice');
      device.capabilities.printer.color = {
        'option': [
          {'type': 'CUSTOM_MONOCHROME', 'vendor_id': '42'},
          {'is_default': true, 'type': 'CUSTOM_COLOR', 'vendor_id': '43'}
        ]
      };
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        checkSectionVisible($('color-settings'), true);
        expectEquals(
            'color',
            $('color-settings').querySelector(
                '.color-settings-select').value);

        return whenAnimationDone('more-settings');
      });
    });

    // Test to verify that duplex settings are set according to the printer
    // capabilities.
    test('DuplexSettingsTrue', function() {
      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        var otherOptions = $('other-options-settings');
        checkSectionVisible(otherOptions, true);
        duplexContainer = otherOptions.querySelector('#duplex-container');
        expectFalse(duplexContainer.hidden);
        expectFalse(duplexContainer.querySelector('.checkbox').checked);

        return whenAnimationDone('more-settings');
      });
    });

    // Test to verify that duplex settings are set according to the printer
    // capabilities.
    test('DuplexSettingsFalse', function() {
      var device = getCddTemplate('FooDevice');
      delete device.capabilities.printer.duplex;
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        // Check that it is collapsed.
        var otherOptions = $('other-options-settings');
        checkSectionVisible(otherOptions, false);

        expandMoreSettings();

        // Now it should be visible.
        checkSectionVisible(otherOptions, true);
        expectTrue(otherOptions.querySelector('#duplex-container').hidden);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that changing the selected printer updates the preview.
    test('PrinterChangeUpdatesPreview', function() {
      return Promise.all([
          setupSettingsAndDestinationsWithCapabilities(),
          nativeLayer.whenCalled('getPreview'),
      ]).then(function(args) {
        expectEquals(0, args[1].requestId);
        expectEquals('FooDevice', args[1].destination.id);
        nativeLayer.resetResolver('getPrinterCapabilities');
        nativeLayer.resetResolver('getPreview');

        // Setup capabilities for BarDevice.
        var device = getCddTemplate('BarDevice');
        device.capabilities.printer.color = {
          'option': [
            {'is_default': true, 'type': 'STANDARD_MONOCHROME'}
          ]
        };
        nativeLayer.setLocalDestinationCapabilities(device);
        // Select BarDevice
        var barDestination =
            printPreview.destinationStore_.destinations().find(
                function(d) {
                  return d.id == 'BarDevice';
                });
        printPreview.destinationStore_.selectDestination(barDestination);
        return Promise.all([
            nativeLayer.whenCalled('getPrinterCapabilities'),
            nativeLayer.whenCalled('getPreview'),
        ]);
      }).then(function(args) {
        expectEquals(1, args[1].requestId);
        expectEquals('BarDevice', args[1].destination.id);
      });
    });

    // Test that error message is displayed when plugin doesn't exist.
    test('NoPDFPluginErrorMessage', function() {
      previewArea.checkPluginCompatibility_ = function() {
        return false;
      }
      nativeLayer.setLocalDestinationCapabilities(getCddTemplate('FooDevice'));
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        var previewAreaEl = $('preview-area');

        var loadingMessageEl =
            previewAreaEl.
            getElementsByClassName('preview-area-loading-message')[0];
        expectTrue(loadingMessageEl.hidden);

        var previewFailedMessageEl = previewAreaEl.getElementsByClassName(
            'preview-area-preview-failed-message')[0];
        expectTrue(previewFailedMessageEl.hidden);

        var printFailedMessageEl =
            previewAreaEl.
            getElementsByClassName('preview-area-print-failed')[0];
        expectTrue(printFailedMessageEl.hidden);

        var customMessageEl =
            previewAreaEl.
            getElementsByClassName('preview-area-custom-message')[0];
        expectFalse(customMessageEl.hidden);
      });
    });

    // Test custom localized paper names.
    test('CustomPaperNames', function() {
      var customLocalizedMediaName = 'Vendor defined localized media name';
      var customMediaName = 'Vendor defined media name';

      var device = getCddTemplate('FooDevice');
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

      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        expandMoreSettings();

        checkSectionVisible($('media-size-settings'), true);
        var mediaSelect =
            $('media-size-settings').querySelector('.settings-select');
        // Check the default media item.
        expectEquals(
            customLocalizedMediaName,
            mediaSelect.options[mediaSelect.selectedIndex].text);
        // Check the other media item.
        expectEquals(
            customMediaName,
            mediaSelect.options[mediaSelect.selectedIndex == 0 ? 1 : 0].text);

        return whenAnimationDone('more-settings');
      });
    });

    // Test advanced settings with 1 capability (should not display settings
    // search box).
    test('AdvancedSettings1Option', function() {
      var device = getCddTemplateWithAdvancedSettings('FooDevice');
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        startAdvancedSettingsTest(device);
        checkElementDisplayed($('advanced-settings').
            querySelector('.search-box-area'), false);

        return whenAnimationDone('more-settings');
      });
    });


    // Test advanced settings with 2 capabilities (should have settings search
    // box).
    test('AdvancedSettings2Options', function() {
      var device = getCddTemplateWithAdvancedSettings('FooDevice');
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
      return setupSettingsAndDestinationsWithCapabilities(device)
          .then(function() {
        startAdvancedSettingsTest(device);

        checkElementDisplayed($('advanced-settings').
          querySelector('.search-box-area'), true);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that initialization with saved destination only issues one call
    // to startPreview.
    test('InitIssuesOneRequest', function() {
      // Load in a bunch of recent destinations with non null capabilities.
      var origin = cr.isChromeOS ? 'chrome_os' : 'local';
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [1, 2, 3].map(function(i) {
          return {
            id: 'ID' + i, origin: origin, account: '',
            capabilities: getCddTemplate('ID' + i), displayName: '',
            extensionId: '', extensionName: ''
          };
        }),
      });

      // Ensure all capabilities are available for fetch.
      nativeLayer.setLocalDestinationCapabilities(getCddTemplate('ID1'));
      nativeLayer.setLocalDestinationCapabilities(getCddTemplate('ID2'))
      nativeLayer.setLocalDestinationCapabilities(getCddTemplate('ID3'));

      // For crbug.com/666595. If multiple destinations are fetched there may
      // be multiple preview requests. This verifies the first fetch is for
      // ID1, which ensures no other destinations are fetched earlier. The last
      // destination retrieved before timeout will end up in the preview
      // request. Ensure this is also ID1.
      setInitialSettings();
      var initialSettingsSet = nativeLayer.whenCalled('getInitialSettings');
      return initialSettingsSet.then(function() {
        return nativeLayer.whenCalled('getPrinterCapabilities');
      }).then(function(id) {
        expectEquals('ID1', id);
        return nativeLayer.whenCalled('getPreview');
      }).then(function(previewArgs) {
        expectEquals(0, previewArgs.requestId);
        expectEquals('ID1', previewArgs.destination.id);
      });
    });

    // Test that invalid settings errors disable the print preview and display
    // an error and that the preview dialog can be recovered by selecting a
    // new destination.
    test('InvalidSettingsError', function() {
      var barDevice = getCddTemplate('BarDevice');
      nativeLayer.setLocalDestinationCapabilities(barDevice);

      // FooDevice is the default printer, so will be selected for the initial
      // preview request.
      nativeLayer.setInvalidPrinterId('FooDevice');
      return Promise.all([
          setupSettingsAndDestinationsWithCapabilities(),
          nativeLayer.whenCalled('getPreview'),
      ]).then(function() {
        // Print preview should have failed with invalid settings, since
        // FooDevice was set as an invalid printer.
        var previewAreaEl = $('preview-area');
        var customMessageEl =
            previewAreaEl.
            getElementsByClassName('preview-area-custom-message')[0];
        expectFalse(customMessageEl.hidden);
        var expectedMessageStart = 'The selected printer is not available or '
            + 'not installed correctly.'
        expectTrue(customMessageEl.textContent.includes(
            expectedMessageStart));

        // Verify that the print button is disabled
        var printButton = $('print-header').querySelector('button.print');
        checkElementDisplayed(printButton, true);
        expectTrue(printButton.disabled);

        // Reset
        nativeLayer.resetResolver('getPrinterCapabilities');
        nativeLayer.resetResolver('getPreview');

        // Select a new destination
        var barDestination =
            printPreview.destinationStore_.destinations().find(
                function(d) {
                  return d.id == 'BarDevice';
                });
        printPreview.destinationStore_.selectDestination(barDestination);
        return Promise.all([
            nativeLayer.whenCalled('getPrinterCapabilities'),
            nativeLayer.whenCalled('getPreview'),
        ]);
      }).then(function() {
        // Has active print button and successfully 'prints', indicating
        // recovery from error state.
        var printButton = $('print-header').querySelector('button.print');
        expectFalse(printButton.disabled);
        printButton.click();
        // This should result in a call to print.
        return nativeLayer.whenCalled('print');
      }).then(
          /**
           * @param {{destination: !print_preview.Destination,
           *          printTicketStore: !print_preview.PrintTicketStore,
           *          cloudPrintInterface: print_preview
           *                                  .CloudPrintInterface,
           *          documentInfo: print_preview.DocumentInfo,
           *          openPdfInPreview: boolean,
           *          showSystemDialog: boolean}} args
           *      The arguments that print() was called with.
           */
          function(args) {
            // Sanity check some printing argument values.
            var printTicketStore = args.printTicketStore;
            expectEquals(barDevice.printer.deviceName, args.destination.id);
            expectEquals(
                getDefaultOrientation(barDevice) == 'LANDSCAPE',
                printTicketStore.landscape.getValue());
            expectEquals(1, printTicketStore.copies.getValueAsNumber());
            var mediaDefault = getDefaultMediaSize(barDevice);
            expectEquals(
                mediaDefault.width_microns,
                printTicketStore.mediaSize.getValue().width_microns);
            expectEquals(
                mediaDefault.height_microns,
                printTicketStore.mediaSize.getValue().height_microns);
            return nativeLayer.whenCalled('hidePreview');
          });
    });

    // Test the preview generator to make sure the generate draft parameter is
    // set correctly. It should be false if the only change is the page range.
    test('GenerateDraft', function() {
      return Promise.all([
          setupSettingsAndDestinationsWithCapabilities(),
          nativeLayer.whenCalled('getPreview'),
      ]).then(function(args) {
        // The first request should generate draft because there was no
        // previous print preview draft.
        expectTrue(args[1].generateDraft);
        expectEquals(0, args[1].requestId);
        nativeLayer.resetResolver('getPreview');

        // Change the page range - no new draft needed.
        printPreview.printTicketStore_.pageRange.updateValue('2');
        return nativeLayer.whenCalled('getPreview');
      }).then(function(args) {
        expectFalse(args.generateDraft);
        expectEquals(1, args.requestId);
        nativeLayer.resetResolver('getPreview');

        // Change the margin type - need to regenerate again.
        printPreview.printTicketStore_.marginsType.updateValue(
            print_preview.ticket_items.MarginsTypeValue.NO_MARGINS);
        return nativeLayer.whenCalled('getPreview');
      }).then(function(args) {
        expectTrue(args.generateDraft);
        expectEquals(2, args.requestId);
      });
    });

    // Test that the policy to use the system default printer by default
    // instead of the most recently used destination works.
    test('SystemDefaultPrinterPolicy', function() {
      // Add recent destination.
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [
          {
            id: 'ID1',
            origin: 'local',
            account: '',
            capabilities: 0,
            displayName: 'One',
            extensionId: '',
            extensionName: '',
          },
        ],
      });

      // Setup local destinations with the system default + recent.
      localDestinationInfos = [
        { printerName: 'One', deviceName: 'ID1' },
        { printerName: 'FooName', deviceName: 'FooDevice' }
      ];
      nativeLayer.setLocalDestinationCapabilities(
          getCddTemplate('ID1', 'One'));

      return setupSettingsAndDestinationsWithCapabilities().then(function() {
        // The system default destination should be used instead of the
        // most recent destination.
        assertEquals(
            'FooDevice',
            printPreview.destinationStore_.selectedDestination.id);
      });
    });

    if (cr.isMac) {
      // Test that Mac "Open PDF in Preview" link is treated correctly as a
      // local printer. See crbug.com/741341 and crbug.com/741528
      test('MacOpenPDFInPreview', function() {
        var device = getPdfPrinter();
        initialSettings.printerName = device.printer.deviceName;
        return setupSettingsAndDestinationsWithCapabilities(device).
            then(function() {
              assertEquals(
                device.printer.deviceName,
                printPreview.destinationStore_.selectedDestination.id);
              return nativeLayer.whenCalled('getPreview');
            }).then(function() {
              var openPdfPreviewLink = $('open-pdf-in-preview-link');
              checkElementDisplayed(openPdfPreviewLink, true);
              openPdfPreviewLink.click();
              // Should result in a print call and dialog should hide
              return nativeLayer.whenCalled('print');
            }).then(
                /**
                 * @param {{destination: !print_preview.Destination,
                 *          printTicketStore: !print_preview.PrintTicketStore,
                 *          cloudPrintInterface: print_preview
                 *                                  .CloudPrintInterface,
                 *          documentInfo: print_preview.DocumentInfo,
                 *          openPdfInPreview: boolean
                 *          showSystemDialog: boolean}} args
                 *      The arguments that print() was called with.
                 */
                function(args) {
                  expectTrue(args.openPdfInPreview);
                  return nativeLayer.whenCalled('hidePreview');
                });
      });

      // Test that the OpenPDFInPreview link is correctly disabled when the
      // print ticket is invalid.
      test('MacOpenPDFInPreviewBadPrintTicket', function() {
        var device = getPdfPrinter();
        initialSettings.printerName = device.printer.deviceName;
        return Promise.all([
          setupSettingsAndDestinationsWithCapabilities(device),
          nativeLayer.whenCalled('getPreview')
        ]).then(function() {
          var openPdfPreviewLink = $('open-pdf-in-preview-link');
          checkElementDisplayed(openPdfPreviewLink, true);
          expectFalse(openPdfPreviewLink.disabled);
          var pageSettings = $('page-settings');
          checkSectionVisible(pageSettings, true);
          nativeLayer.resetResolver('getPreview');

          // Set page settings to a bad value
          pageSettings.querySelector('.page-settings-custom-input').value =
              'abc';
          pageSettings.querySelector('.page-settings-custom-radio').click();

          // No new preview
          nativeLayer.whenCalled('getPreview').then(function() {
            assertTrue(false);
          });

          // Expect disabled print button and Pdf in preview link
          var printButton = $('print-header').querySelector('button.print');
          checkElementDisplayed(printButton, true);
          expectTrue(printButton.disabled);
          checkElementDisplayed(openPdfPreviewLink, true);
          expectTrue(openPdfPreviewLink.disabled);
        });
      });
    }  // cr.isMac

    // Test that the system dialog link works correctly on Windows
    if (cr.isWindows) {
      test('WinSystemDialogLink', function() {
        return setupSettingsAndDestinationsWithCapabilities().
            then(function() {
              assertEquals(
                'FooDevice',
                printPreview.destinationStore_.selectedDestination.id);
              return nativeLayer.whenCalled('getPreview');
            }).then(function() {
              var systemDialogLink = $('system-dialog-link');
              checkElementDisplayed(systemDialogLink, true);
              systemDialogLink.click();
              // Should result in a print call and dialog should hide
              return nativeLayer.whenCalled('print');
            }).then(
                /**
                 * @param {{destination: !print_preview.Destination,
                 *          printTicketStore: !print_preview.PrintTicketStore,
                 *          cloudPrintInterface: print_preview
                 *                                  .CloudPrintInterface,
                 *          documentInfo: print_preview.DocumentInfo,
                 *          openPdfInPreview: boolean
                 *          showSystemDialog: boolean}} args
                 *      The arguments that print() was called with.
                 */
                function(args) {
                  expectTrue(args.showSystemDialog);
                  return nativeLayer.whenCalled('hidePreview');
                });
      });

      // Test that the System Dialog link is correctly disabled when the
      // print ticket is invalid.
      test('WinSystemDialogLinkBadPrintTicket', function() {
        return Promise.all([
          setupSettingsAndDestinationsWithCapabilities(),
          nativeLayer.whenCalled('getPreview')
        ]).then(function() {
          var systemDialogLink = $('system-dialog-link');
          checkElementDisplayed(systemDialogLink, true);
          expectFalse(systemDialogLink.disabled);

          var pageSettings = $('page-settings');
          checkSectionVisible(pageSettings, true);
          nativeLayer.resetResolver('getPreview');

          // Set page settings to a bad value
          pageSettings.querySelector('.page-settings-custom-input').value =
              'abc';
          pageSettings.querySelector('.page-settings-custom-radio').click();

          // No new preview
          nativeLayer.whenCalled('getPreview').then(function() {
            assertTrue(false);
          });

          // Expect disabled print button and Pdf in preview link
          var printButton = $('print-header').querySelector('button.print');
          checkElementDisplayed(printButton, true);
          expectTrue(printButton.disabled);
          checkElementDisplayed(systemDialogLink, true);
          expectTrue(systemDialogLink.disabled);
        });
      });
    }  // cr.isWindows
  });
});
