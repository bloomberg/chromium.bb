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
  });
});
