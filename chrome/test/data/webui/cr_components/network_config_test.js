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
      networkConfigShare: '',
      networkCAUseDefault: '',
      networkCADoNotCheck: '',
      networkConfigSaveCredentials: '',
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
      return new Promise(resolve => {
        networkConfig.async(resolve);
      }).then(() => {
        assertEquals('someguid', networkConfig.networkProperties.GUID);
        assertEquals('somename', networkConfig.networkProperties.Name);
        assertTrue(!!networkConfig.$$('#share'));
        assertTrue(!!networkConfig.$$('#ssid'));
        assertTrue(!!networkConfig.$$('#security'));
        assertTrue(networkConfig.$$('#security').disabled);
      });
    });
  });
});
