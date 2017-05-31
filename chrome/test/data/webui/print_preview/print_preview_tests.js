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
   * |initialSettings|. Creates |printPreview| if it does not
   * already exist.
   */
  function setInitialSettings() {
    nativeLayer.setInitialSettings(initialSettings);
    printPreview.initialize();
  }

  /**
   * Dispatch the LOCAL_DESTINATIONS_SET event. This call is NOT async and will
   * happen in the same thread.
   */
  function setLocalDestinations() {
    var localDestsSetEvent =
        new Event(print_preview.NativeLayer.EventType.LOCAL_DESTINATIONS_SET);
    localDestsSetEvent.destinationInfos = localDestinationInfos;
    nativeLayer.getEventTarget().dispatchEvent(localDestsSetEvent);
  }

  /**
   * Dispatch the CAPABILITIES_SET event. This call is NOT async and will
   * happen in the same thread.
   * @param {!Object} device The device whose capabilities should be dispatched.
   */
  function setCapabilities(device) {
    var capsSetEvent =
        new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
    capsSetEvent.settingsInfo = device;
    nativeLayer.getEventTarget().dispatchEvent(capsSetEvent);
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
   * @return {!Object}
   */
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
        element.removeEventListener(f, 'animationend');
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
        return false;
      };
    });

    setup(function() {
      initialSettings = new print_preview.NativeInitialSettings(
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

      localDestinationInfos = [
        { printerName: 'FooName', deviceName: 'FooDevice' },
        { printerName: 'BarName', deviceName: 'BarDevice' },
      ];

      nativeLayer = new print_preview.NativeLayerStub();
      print_preview.NativeLayer.setInstance(nativeLayer);
      printPreview = new print_preview.PrintPreview();
      previewArea = printPreview.getPreviewArea();
    });

    // Test some basic assumptions about the print preview WebUI.
    test('PrinterList', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
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
      setInitialSettings();

      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();

            var cloudPrintEnableEvent = new Event(
                print_preview.NativeLayer.EventType.CLOUD_PRINT_ENABLE);
            cloudPrintEnableEvent.baseCloudPrintUrl = 'cloudprint url';
            nativeLayer.getEventTarget().dispatchEvent(
                cloudPrintEnableEvent);

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
      initialSettings.serializedAppStateStr_ = JSON.stringify({
        version: 2,
        recentDestinations: [
          {
            id: 'ID',
            origin: cr.isChromeOS ? 'chrome_os' : 'local',
            account: '',
            capabilities: 0,
            name: '',
            extensionId: '',
            extensionName: '',
          },
        ],
      });

      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings');
    });

    test('RestoreMultipleDestinations', function() {
      var origin = cr.isChromeOS ? 'chrome_os' : 'local';

      initialSettings.serializedAppStateStr_ = JSON.stringify({
        version: 2,
        recentDestinations: [
          {
            id: 'ID1',
            origin: origin,
            account: '',
            capabilities: 0,
            name: '',
            extensionId: '',
            extensionName: '',
          }, {
            id: 'ID2',
            origin: origin,
            account: '',
            capabilities: 0,
            name: '',
            extensionId: '',
            extensionName: '',
          }, {
            id: 'ID3',
            origin: origin,
            account: '',
            capabilities: 0,
            name: '',
            extensionId: '',
            extensionName: '',
          },
        ],
      });

      setInitialSettings();

      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            // Set capabilities for the three recently used destinations + 1
            // more.
            setCapabilities(getCddTemplate('ID1'));
            setCapabilities(getCddTemplate('ID2'));
            setCapabilities(getCddTemplate('ID3'));
            setCapabilities(getCddTemplate('ID4'));

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
      initialSettings.serializedDefaultDestinationSelectionRulesStr_ =
          JSON.stringify({namePattern: '.*Bar.*'});
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
            assertEquals(
                'BarDevice',
                printPreview.destinationStore_.selectedDestination.id);
          });
    });

    test('SystemDialogLinkIsHiddenInAppKioskMode', function() {
      if (!cr.isChromeOS)
        initialSettings.isInAppKioskMode_ = true;

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

      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
            var device = getCddTemplate('FooDevice');
            device.capabilities.printer.color = {
              option: [{is_default: true, type: 'STANDARD_COLOR'}]
            };
            delete device.capabilities.printer.copies;
            setCapabilities(device);

            checkSectionVisible($('layout-settings'), true);
            checkSectionVisible($('color-settings'), false);
            checkSectionVisible($('copies-settings'), false);

            return whenAnimationDone('other-options-collapsible');
          });
    });

    // When the source is 'PDF' and 'Save as PDF' option is selected, we hide
    // the fit to page option.
    test('PrintToPDFSelectedCapabilities', function() {
      // Add PDF printer.
      initialSettings.isDocumentModifiable_ = false;
      initialSettings.systemDefaultDestinationId_ = 'Save as PDF';
      setInitialSettings();

      return nativeLayer.whenCalled('getInitialSettings').then(function() {
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
        setCapabilities(device);

        var otherOptions = $('other-options-settings');
        // If rasterization is an option, other options should be visible. If
        // not, there should be no available other options.
        checkSectionVisible(otherOptions, isPrintAsImageEnabled());
        if (isPrintAsImageEnabled()) {
          checkElementDisplayed(
              otherOptions.querySelector('#fit-to-page-container'), false);
          checkElementDisplayed(
              otherOptions.querySelector('#rasterize-container'), true);
        }
        checkSectionVisible($('media-size-settings'), false);
        checkSectionVisible($('scaling-settings'), false);
      });
    });

    // When the source is 'HTML', we always hide the fit to page option and show
    // media size option.
    test('SourceIsHTMLCapabilities', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();
        setCapabilities(getCddTemplate('FooDevice'));

        var otherOptions = $('other-options-settings');
        var fitToPage = otherOptions.querySelector('#fit-to-page-container');
        var rasterize;
        if (isPrintAsImageEnabled())
          rasterize = otherOptions.querySelector('#rasterize-container');
        var mediaSize = $('media-size-settings');
        var scalingSettings = $('scaling-settings');

        // Check that options are collapsed (section is visible, because duplex
        // is available).
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

    // When the source is "PDF", depending on the selected destination printer,
    // we show/hide the fit to page option and hide media size selection.
    test('SourceIsPDFCapabilities', function() {
      initialSettings.isDocumentModifiable_ = false;
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
            setCapabilities(getCddTemplate('FooDevice'));

            var otherOptions = $('other-options-settings');
            var scalingSettings = $('scaling-settings');
            var fitToPageContainer =
                otherOptions.querySelector('#fit-to-page-container');
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

    // When the source is "PDF", depending on the selected destination printer,
    // we show/hide the fit to page option and hide media size selection.
    test('ScalingUnchecksFitToPage', function() {
      initialSettings.isDocumentModifiable_ = false;
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
            setCapabilities(getCddTemplate('FooDevice'));

            var otherOptions = $('other-options-settings');
            var scalingSettings = $('scaling-settings');

            checkSectionVisible(otherOptions, true);
            var fitToPageContainer =
                otherOptions.querySelector('#fit-to-page-container');
            checkElementDisplayed(fitToPageContainer, true);
            expectTrue(
                fitToPageContainer.querySelector('.checkbox').checked);
            expandMoreSettings();
            checkSectionVisible($('media-size-settings'), true);
            checkSectionVisible(scalingSettings, true);

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
            expectFalse(
                fitToPageContainer.querySelector('.checkbox').checked);

            return whenAnimationDone('other-options-collapsible');
          });
    });

    // When the number of copies print preset is set for source 'PDF', we update
    // the copies value if capability is supported by printer.
    test('CheckNumCopiesPrintPreset', function() {
      initialSettings.isDocumentModifiable_ = false;
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();
        setCapabilities(getCddTemplate('FooDevice'));

        // Indicate that the number of copies print preset is set for source
        // PDF.
        var printPresetOptions = {
          disableScaling: true,
          copies: 2
        };
        var printPresetOptionsEvent = new Event(
            print_preview.NativeLayer.EventType.PRINT_PRESET_OPTIONS);
        printPresetOptionsEvent.optionsFromDocument = printPresetOptions;
        nativeLayer.getEventTarget().
            dispatchEvent(printPresetOptionsEvent);

        checkSectionVisible($('copies-settings'), true);
        expectEquals(
            printPresetOptions.copies,
            parseInt($('copies-settings').querySelector('.user-value').value));

        return whenAnimationDone('other-options-collapsible');
      });
    });

    // When the duplex print preset is set for source 'PDF', we update the
    // duplex setting if capability is supported by printer.
    test('CheckDuplexPrintPreset', function() {
      initialSettings.isDocumentModifiable_ = false;
      setInitialSettings();

      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
            setCapabilities(getCddTemplate('FooDevice'));

            // Indicate that the duplex print preset is set to "long edge" for
            // source PDF.
            var printPresetOptions = {
              duplex: 1
            };
            var printPresetOptionsEvent = new Event(
                print_preview.NativeLayer.EventType.PRINT_PRESET_OPTIONS);
            printPresetOptionsEvent.optionsFromDocument = printPresetOptions;
            nativeLayer.getEventTarget().
                dispatchEvent(printPresetOptionsEvent);

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
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
            setCapabilities(getCddTemplate('FooDevice'));

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
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();
            setCapabilities(getCddTemplate('FooDevice'));

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
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();
        setCapabilities(getCddTemplate('FooDevice'));

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
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();
        setCapabilities(getCddTemplate('FooDevice'));

        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because duplex
        // is available).
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
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();
        setCapabilities(getCddTemplate('FooDevice'));

        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because duplex
        // is available).
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
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();
        var device = getCddTemplate('FooDevice');
        device.capabilities.printer.media_size = {
          'option': [
            {'name': 'SmallLabel', 'width_microns': 38100,
              'height_microns': 12700, 'is_default': false},
            {'name': 'BigLabel', 'width_microns': 50800,
              'height_microns': 76200, 'is_default': true}
          ]
        };
        setCapabilities(device);

        var otherOptions = $('other-options-settings');
        var headerFooter =
            otherOptions.querySelector('#header-footer-container');

        // Check that options are collapsed (section is visible, because duplex
        // is available).
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
    test('TestColorSettingsMonochrome', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();

        // Only one option, standard monochrome.
        var device = getCddTemplate('FooDevice');
        device.capabilities.printer.color = {
          'option': [
            {'is_default': true, 'type': 'STANDARD_MONOCHROME'}
          ]
        };
        setCapabilities(device);

        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, one option, custom monochrome.
    test('TestColorSettingsCustomMonochrome', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();

        // Only one option, standard monochrome.
        var device = getCddTemplate('FooDevice');
        device.capabilities.printer.color = {
          'option': [
            {'is_default': true, 'type': 'CUSTOM_MONOCHROME',
             'vendor_id': '42'}
          ]
        };
        setCapabilities(device);

        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, one option, standard color.
    test('TestColorSettingsColor', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();

        var device = getCddTemplate('FooDevice');
        device.capabilities.printer.color = {
          'option': [
            {'is_default': true, 'type': 'STANDARD_COLOR'}
          ]
        };
        setCapabilities(device);

        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, one option, custom color.
    test('TestColorSettingsCustomColor', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();

        var device = getCddTemplate('FooDevice');
        device.capabilities.printer.color = {
          'option': [
            {'is_default': true, 'type': 'CUSTOM_COLOR', 'vendor_id': '42'}
          ]
        };
        setCapabilities(device);

        checkSectionVisible($('color-settings'), false);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, two options, both standard, defaults to
    // color.
    test('TestColorSettingsBothStandardDefaultColor', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();

        var device = getCddTemplate('FooDevice');
        device.capabilities.printer.color = {
          'option': [
            {'type': 'STANDARD_MONOCHROME'},
            {'is_default': true, 'type': 'STANDARD_COLOR'}
          ]
        };
        setCapabilities(device);

        checkSectionVisible($('color-settings'), true);
        expectEquals(
            'color',
            $('color-settings').querySelector('.color-settings-select').value);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, two options, both standard, defaults to
    // monochrome.
    test('TestColorSettingsBothStandardDefaultMonochrome', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(function() {
        setLocalDestinations();

        var device = getCddTemplate('FooDevice');
        device.capabilities.printer.color = {
          'option': [
            {'is_default': true, 'type': 'STANDARD_MONOCHROME'},
            {'type': 'STANDARD_COLOR'}
          ]
        };
        setCapabilities(device);

        checkSectionVisible($('color-settings'), true);
        expectEquals(
            'bw',
            $('color-settings').querySelector('.color-settings-select').value);

        return whenAnimationDone('more-settings');
      });
    });

    // Test that the color settings, two options, both custom, defaults to
    // color.
    test('TestColorSettingsBothCustomDefaultColor', function() {
      setInitialSettings();
      return nativeLayer.whenCalled('getInitialSettings').then(
          function() {
            setLocalDestinations();

            var device = getCddTemplate('FooDevice');
            device.capabilities.printer.color = {
              'option': [
                {'type': 'CUSTOM_MONOCHROME', 'vendor_id': '42'},
                {'is_default': true, 'type': 'CUSTOM_COLOR', 'vendor_id': '43'}
              ]
            };
            setCapabilities(device);

            checkSectionVisible($('color-settings'), true);
            expectEquals(
                'color',
                $('color-settings').querySelector(
                    '.color-settings-select').value);

            return whenAnimationDone('more-settings');
          });
        });

  });
});
