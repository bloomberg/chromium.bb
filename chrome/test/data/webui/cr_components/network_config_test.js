// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

suite('network-config', function() {
  var networkConfig;

  /** @type {NetworkingPrivate} */
  var api_;

  suiteSetup(function() {
    api_ = new chrome.FakeNetworkingPrivate();
    loadTimeData.data = {
      networkCADoNotCheck: '',
      networkCAUseDefault: '',
      networkCertificateName: '',
      networkCertificateNameHardwareBacked: '',
      networkCertificateNoneInstalled: '',
      networkConfigSaveCredentials: '',
      networkConfigShare: '',
      showPassword: '',
    };
    CrOncStrings.overrideValues();
  });

  function setNetworkConfig(networkProperties) {
    PolymerTest.clearBody();
    networkConfig = document.createElement('network-config');
    networkConfig.networkingPrivate = api_;
    networkConfig.networkProperties = networkProperties;
    document.body.appendChild(networkConfig);
    networkConfig.init();
    Polymer.dom.flush();
  };

  function flushAsync() {
    Polymer.dom.flush();
    return new Promise(resolve => {
      networkConfig.async(resolve);
    });
  };

  suite('New WiFi Config', function() {
    setup(function() {
      api_.resetForTest();
      setNetworkConfig({GUID: '', Name: '', Type: 'WiFi'});
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      assertTrue(!!networkConfig.$$('#share'));
      assertTrue(!!networkConfig.$$('#ssid'));
      assertTrue(!!networkConfig.$$('#security'));
      assertFalse(networkConfig.$$('#security').disabled);
    });
  });

  suite('Existing WiFi Config', function() {
    setup(function() {
      api_.resetForTest();
      var network = {
        GUID: 'someguid',
        Name: 'somename',
        Type: 'WiFi',
        WiFi: {SSID: 'somessid', Security: 'None'}
      };
      api_.addNetworksForTest([network]);
      setNetworkConfig({GUID: 'someguid', Name: '', Type: 'WiFi'});
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    test('Default', function() {
      return flushAsync().then(() => {
        assertEquals('someguid', networkConfig.networkProperties.GUID);
        assertEquals('somename', networkConfig.networkProperties.Name);
        assertTrue(!!networkConfig.$$('#share'));
        assertTrue(!!networkConfig.$$('#ssid'));
        assertTrue(!!networkConfig.$$('#security'));
        assertTrue(networkConfig.$$('#security').disabled);
      });
    });
  });

  suite('Share', function() {
    setup(function() {
      api_.resetForTest();
    });

    teardown(function() {
      PolymerTest.clearBody();
    });

    function setLoginOrGuest() {
      // Networks must be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = true;
    };

    function setKiosk() {
      // New networks can not be shared.
      networkConfig.shareAllowEnable = false;
      networkConfig.shareDefault = false;
    };

    function setAuthenticated() {
      // Logged in users can share new networks.
      networkConfig.shareAllowEnable = true;
      // Authenticated networks default to not shared.
      networkConfig.shareDefault = false;
    };

    test('New Config: Login or guest', function() {
      setNetworkConfig({GUID: '', Name: '', Type: 'WiFi'});
      setLoginOrGuest();
      // Insecure networks are always shared so test a secure config.
      networkConfig.security_ = 'WEP-PSK';
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Kiosk', function() {
      setNetworkConfig({GUID: '', Name: '', Type: 'WiFi'});
      setKiosk();
      // Insecure networks are always shared so test a secure config.
      networkConfig.security_ = 'WEP-PSK';
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertFalse(share.checked);
      });
    });

    test('New Config: Authenticated, Not secure', function() {
      setNetworkConfig({GUID: '', Name: '', Type: 'WiFi'});
      setAuthenticated();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('New Config: Authenticated, Secure', function() {
      setNetworkConfig({GUID: '', Name: '', Type: 'WiFi'});
      setAuthenticated();
      networkConfig.security_ = 'WEP-PSK';
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertFalse(share.disabled);
        assertFalse(share.checked);
      });
    });

    // Existing networks can not change their shared state.

    test('Existing Shared', function() {
      var network = {
        GUID: 'someguid',
        Name: 'somename',
        Source: 'Device',
        Type: 'WiFi',
        WiFi: {SSID: 'somessid', Security: 'None'}
      };
      api_.addNetworksForTest([network]);
      setNetworkConfig({GUID: 'someguid', Name: '', Type: 'WiFi'});
      setAuthenticated();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertTrue(share.checked);
      });
    });

    test('Existing Not Shared', function() {
      var network = {
        GUID: 'someguid',
        Name: 'somename',
        Source: 'User',
        Type: 'WiFi',
        WiFi: {SSID: 'somessid', Security: 'WEP-PSK'}
      };
      api_.addNetworksForTest([network]);
      setNetworkConfig({GUID: 'someguid', Name: '', Type: 'WiFi'});
      setAuthenticated();
      return flushAsync().then(() => {
        let share = networkConfig.$$('#share');
        assertTrue(!!share);
        assertTrue(share.disabled);
        assertFalse(share.checked);
      });
    });

  });

});
