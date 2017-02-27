// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Internet', function() {
  /** @type {InternetPageElement} */
  var internetPage = null;

  /** @type {NetworkSummaryElement} */
  var networkSummary_ = null;

  /** @type {NetworkingPrivate} */
  var networkingPrivateApi_;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      internetAddConnection: 'internetAddConnection',
      internetAddConnectionExpandA11yLabel:
          'internetAddConnectionExpandA11yLabel',
      internetAddConnectionNotAllowed: 'internetAddConnectionNotAllowed',
      internetAddThirdPartyVPN: 'internetAddThirdPartyVPN',
      internetAddVPN: 'internetAddVPN',
      internetAddWiFi: 'internetAddWiFi',
      internetDetailPageTitle: 'internetDetailPageTitle',
      internetKnownNetworksPageTitle: 'internetKnownNetworksPageTitle',
    });

    CrOncStrings = {
      OncTypeCellular: 'OncTypeCellular',
      OncTypeEthernet: 'OncTypeEthernet',
      OncTypeVPN: 'OncTypeVPN',
      OncTypeWiFi: 'OncTypeWiFi',
      OncTypeWiMAX: 'OncTypeWiMAX',
      networkDisabled: 'networkDisabled',
      networkListItemConnected: 'networkListItemConnected',
      networkListItemConnecting: 'networkListItemConnecting',
      networkListItemConnectingTo: 'networkListItemConnectingTo',
      networkListItemNotConnected: 'networkListItemNotConnected',
      vpnNameTemplate: 'vpnNameTemplate',
    };

    networkingPrivateApi_ = new settings.FakeNetworkingPrivate();

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  suite('MainPage', function() {
    setup(function() {
      PolymerTest.clearBody();
      internetPage = document.createElement('settings-internet-page');
      assertTrue(!!internetPage);
      networkingPrivateApi_.resetForTest();
      internetPage.networkingPrivate = networkingPrivateApi_;
      document.body.appendChild(internetPage);
      networkSummary_ = internetPage.$$('network-summary');
      assertTrue(!!networkSummary_);
      Polymer.dom.flush();
    });

    teardown(function() {
      internetPage.remove();
    });

    test('Ethernet', function() {
      // Default fake device state is Ethernet enabled only.
      var ethernet = networkSummary_.$$('#Ethernet');
      assertTrue(!!ethernet);
      assertEquals(1, ethernet.networkStateList.length);
      assertEquals(null, networkSummary_.$$('#Cellular'));
      assertEquals(null, networkSummary_.$$('#VPN'));
      assertEquals(null, networkSummary_.$$('#WiMAX'));
      assertEquals(null, networkSummary_.$$('#WiFi'));
    });

    test('WiFi', function() {
      networkingPrivateApi_.addNetworksForTest([
        {GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'},
        {GUID: 'wifi12_guid', Name: 'wifi2', Type: 'WiFi'},
      ]);
      networkingPrivateApi_.enableNetworkType('WiFi');
      Polymer.dom.flush();
      var wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);
      assertEquals(2, wifi.networkStateList.length);
    });

    test('WiFiToggle', function() {
      var api = networkingPrivateApi_;

      // Make WiFi an available but disabled technology.
      api.disableNetworkType('WiFi');
      Polymer.dom.flush();
      var wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);

      // Ensure that the initial state is disabled and the toggle is
      // enabled but unchecked.
      assertEquals('Disabled', api.getDeviceStateForTest('WiFi').State);
      var toggle = wifi.$$('#deviceEnabledButton');
      assertTrue(!!toggle);
      assertTrue(toggle.enabled);
      assertFalse(toggle.checked);

      // Tap the enable toggle button and ensure the state becomes enabled.
      MockInteractions.tap(toggle);
      Polymer.dom.flush();
      assertTrue(toggle.checked);
      assertEquals('Enabled', api.getDeviceStateForTest('WiFi').State);
    });
  });
});
