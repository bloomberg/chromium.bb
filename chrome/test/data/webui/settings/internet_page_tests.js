// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('Internet', function() {
  /** @type {InternetPageElement} */
  var internetPage = null;

  /** @type {NetworkSummaryElement} */
  var networkSummary_ = null;

  /** @type {NetworkingPrivate} */
  var api_;

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

    api_ = new settings.FakeNetworkingPrivate();

    // Disable animations so sub-pages open within one event loop.
    testing.Test.disableAnimationsAndTransitions();
  });

  setup(function() {
    PolymerTest.clearBody();
    internetPage = document.createElement('settings-internet-page');
    assertTrue(!!internetPage);
    api_.resetForTest();
    internetPage.networkingPrivate = api_;
    document.body.appendChild(internetPage);
    networkSummary_ = internetPage.$$('network-summary');
    assertTrue(!!networkSummary_);
    Polymer.dom.flush();
  });

  teardown(function() {
    internetPage.remove();
  });

  suite('MainPage', function() {
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
      api_.addNetworksForTest([
        {GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'},
        {GUID: 'wifi12_guid', Name: 'wifi2', Type: 'WiFi'},
      ]);
      api_.enableNetworkType('WiFi');
      Polymer.dom.flush();
      var wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);
      assertEquals(2, wifi.networkStateList.length);
    });

    test('WiFiToggle', function() {
      // Make WiFi an available but disabled technology.
      api_.disableNetworkType('WiFi');
      Polymer.dom.flush();
      var wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);

      // Ensure that the initial state is disabled and the toggle is
      // enabled but unchecked.
      assertEquals('Disabled', api_.getDeviceStateForTest('WiFi').State);
      var toggle = wifi.$$('#deviceEnabledButton');
      assertTrue(!!toggle);
      assertTrue(toggle.enabled);
      assertFalse(toggle.checked);

      // Tap the enable toggle button and ensure the state becomes enabled.
      MockInteractions.tap(toggle);
      Polymer.dom.flush();
      assertTrue(toggle.checked);
      assertEquals('Enabled', api_.getDeviceStateForTest('WiFi').State);
    });
  });

  suite('SubPage', function() {
    test('WiFi', function() {
      api_.addNetworksForTest([
        {GUID: 'wifi1_guid', Name: 'wifi1', Type: 'WiFi'},
        {GUID: 'wifi12_guid', Name: 'wifi2', Type: 'WiFi'},
      ]);
      api_.enableNetworkType('WiFi');
      Polymer.dom.flush();
      var wifi = networkSummary_.$$('#WiFi');
      assertTrue(!!wifi);
      MockInteractions.tap(wifi.$$('button.subpage-arrow'));
      var subpage = internetPage.$$('settings-internet-subpage');
      assertTrue(!!subpage);
      assertEquals(2, subpage.networkStateList_.length);
      var networkList = subpage.$$('#networkList');
      assertTrue(!!networkList);
      assertEquals(2, networkList.networks.length);
    });

    test('VPN', function() {
      api_.addNetworksForTest([
        {GUID: 'vpn1_guid', Name: 'vpn1', Type: 'VPN'},
        {GUID: 'vpn2_guid', Name: 'vpn1', Type: 'VPN'},
        {
          GUID: 'third_party1_vpn1_guid',
          Name: 'vpn3',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id1', ProviderName: 'pname1'}
          }
        },
        {
          GUID: 'third_party1_vpn2_guid',
          Name: 'vpn4',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id1', ProviderName: 'pname1'}
          }
        },
        {
          GUID: 'third_party2_vpn1_guid',
          Name: 'vpn5',
          Type: 'VPN',
          VPN: {
            Type: 'ThirdPartyVPN',
            ThirdPartyVPN: {ExtensionID: 'id2', ProviderName: 'pname2'}
          }
        },
      ]);
      api_.onNetworkListChanged.callListeners();
      Polymer.dom.flush();
      var vpn = networkSummary_.$$('#VPN');
      assertTrue(!!vpn);
      MockInteractions.tap(vpn.$$('button.subpage-arrow'));
      var subpage = internetPage.$$('settings-internet-subpage');
      assertTrue(!!subpage);
      assertEquals(2, subpage.networkStateList_.length);
      var networkList = subpage.$$('#networkList');
      assertTrue(!!networkList);
      assertEquals(2, networkList.networks.length);
      // TODO(stevenjb): Implement fake management API and test third
      // party provider sections.
    });
  });
});
