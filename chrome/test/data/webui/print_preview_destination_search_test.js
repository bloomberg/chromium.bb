// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ROOT_PATH = '../../../../';

GEN_INCLUDE(
        [ROOT_PATH + 'chrome/test/data/webui/polymer_browser_test_base.js']);

/**
 * Test fixture for DestinationSearch of Print Preview.
 * @constructor
 * @extends {PolymerTest}
 */
function PrintPreviewDestinationSearchTest() {}

PrintPreviewDestinationSearchTest.prototype = {
  __proto__: PolymerTest.prototype,

  /** @override */
  browsePreload: 'chrome://print',

  /** @override */
  runAccessibilityChecks: false,

  /** @override */
  extraLibraries: PolymerTest.getLibraries(ROOT_PATH),
};

TEST_F('PrintPreviewDestinationSearchTest', 'Select', function() {
  var self = this;

  suite('DestinationSearchTest', function() {
    var root_;

    var destinationSearch_;
    var nativeLayer_;
    var invitationStore_;
    var destinationStore_;
    var userInfo_;

    function getCaps() {
      return {
        'printer': {
          'color': {
            'option': [{
              'is_default': true,
              'type': 'STANDARD_MONOCHROME',
              'vendor_id': '13'
            }]
          },
          'copies': {},
          'duplex': {
            'option': [
              {'type': 'NO_DUPLEX'}, {'is_default': true, 'type': 'LONG_EDGE'},
              {'type': 'SHORT_EDGE'}
            ]
          },
          'media_size': {
            'option': [
              {
                'custom_display_name': 'na letter',
                'height_microns': 279400,
                'is_default': true,
                'name': 'NA_LETTER',
                'vendor_id': 'na_letter_8.5x11in',
                'width_microns': 215900
              },
              {
                'custom_display_name': 'na legal',
                'height_microns': 355600,
                'name': 'NA_LEGAL',
                'vendor_id': 'na_legal_8.5x14in',
                'width_microns': 215900
              }
            ]
          },
          'page_orientation': {
            'option': [
              {'is_default': true, 'type': 'PORTRAIT'}, {'type': 'LANDSCAPE'},
              {'type': 'AUTO'}
            ]
          },
          'supported_content_type': [{'content_type': 'application/pdf'}]
        },
        'version': '1.0'
      };
    };

    function waitForEvent(element, eventName) {
      return new Promise(function(resolve) {
        var listener = function(e) {
          resolve();
          element.removeEventListener(eventName, listener);
        };

        element.addEventListener(eventName, listener);
      });
    };

    function mockSetupCall(destId, nativeLayerMock) {
      var resolver = new PromiseResolver();

      if (cr.isChromeOS) {
        nativeLayerMock.expects(once()).setupPrinter(destId).
            will(returnValue(resolver.promise));

        return resolver;
      }

      nativeLayerMock.expects(once()).startGetLocalDestinationCapabilities(
          destId);
      resolver.promise.then(
          function(result) {
            // Simulate the native layer dispatching capabilities.
            var capsSetEvent =
                new Event(print_preview.NativeLayer.EventType.CAPABILITIES_SET);
            capsSetEvent.settingsInfo = result;
            destinationStore_.onLocalDestinationCapabilitiesSet_(capsSetEvent);
          }.bind(this),
          function() {
            var failEvent = new Event(
                print_preview.NativeLayer.EventType.GET_CAPABILITIES_FAIL);
            failEvent.destinationId = destId;
            destinationStore_.onGetCapabilitiesFail_(failEvent);
          }.bind(this));

      return resolver;
    };

    function requestSetup(destId, destinationSearch) {
      var origin = cr.isChromeOS ? print_preview.DestinationOrigin.CROS :
                                   print_preview.DestinationOrigin.LOCAL;

      var dest = new print_preview.Destination(destId,
          print_preview.DestinationType.LOCAL,
          origin,
          "displayName",
          print_preview.DestinationConnectionStatus.ONLINE);

      // Add the destination to the list.
      destinationSearch.localList_.updateDestinations([dest]);

      // Select destination.
      if (cr.isChromeOS) {
        destinationSearch.handleConfigureDestination_(dest);
      } else {
        destinationSearch.handleOnDestinationSelect_(dest);
      }
    };

    function resolveSetup(resolver, printerId, success, capabilities) {
      var response = {
        printerId: printerId,
        capabilities: capabilities,
        success: success
      };

      resolver.resolve(response);
    }

    setup(function() {
      Mock4JS.clearMocksToVerify();

      nativeLayer_ = mock(print_preview.NativeLayer);
      nativeLayer_.expects(atLeastOnce())
          .addEventListener(ANYTHING, ANYTHING, ANYTHING);

      invitationStore_ = new print_preview.InvitationStore();
      destinationStore_ = new print_preview.DestinationStore(
          nativeLayer_.proxy(), new print_preview.UserInfo(),
          new print_preview.AppState());
      userInfo_ = new print_preview.UserInfo();

      destinationSearch_ = new print_preview.DestinationSearch(
          destinationStore_, invitationStore_, userInfo_);
      destinationSearch_.decorate($('destination-search'));
    });

    teardown(function() {
      Mock4JS.verifyAllMocks();
    });

    test('ResolutionFails', function() {
      var destId = "001122DEADBEEF";

      var resolver = mockSetupCall(destId, nativeLayer_);
      requestSetup(destId, destinationSearch_);
      resolver.reject(destId);
    });

    test('ReceiveSuccessfulSetup', function() {
      var destId = "00112233DEADBEEF";

      var waiter = waitForEvent(
          destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT);

      var resolver = mockSetupCall(destId, nativeLayer_);

      requestSetup(destId, destinationSearch_);
      resolveSetup(resolver, destId, true, getCaps());

      // wait for event propogation to complete.
      return waiter.then(function() {
        // after setup succeeds, the destination should be selected.
        assertNotEquals(null, destinationStore_.selectedDestination);
        assertEquals(destId, destinationStore_.selectedDestination.id);
      });
    });

    if (cr.isChromeOS) {
      // The 'ResolutionFails' test covers this case for non-CrOS.
      test('ReceiveFailedSetup', function() {
        var destId = '00112233DEADBEEF';

        var resolver = mockSetupCall(destId, nativeLayer_);
        requestSetup(destId, destinationSearch_);

        // Force resolution to fail.
        resolveSetup(resolver, destId, false, null);

        // Selection should not change on ChromeOS.
        assertEquals(null, destinationStore_.selectedDestination);
      });
    }

    test('CloudKioskPrinter', function() {
      var printerId = 'cloud-printer-id';

      // Create cloud destination.
      var cloudDest = new print_preview.Destination(printerId,
          print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.DEVICE,
          "displayName",
          print_preview.DestinationConnectionStatus.ONLINE);
      cloudDest.capabilities = getCaps();

      // Place destination in the local list as happens for Kiosk printers.
      destinationSearch_.localList_.updateDestinations([cloudDest]);
      var dest = destinationSearch_.localList_.getDestinationItem(printerId);
      // Simulate a click.
      dest.onActivate_();

      // Verify that the destination has been selected.
      assertEquals(printerId, destinationStore_.selectedDestination.id);
    });
  });

  mocha.run();
});
