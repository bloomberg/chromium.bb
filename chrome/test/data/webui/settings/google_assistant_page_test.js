// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {settings.GoogleAssistantBrowserProxy}
 */
class TestGoogleAssistantBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'setGoogleAssistantEnabled',
      'setGoogleAssistantContextEnabled',
      'showGoogleAssistantSettings',
    ]);
  }

  /** @override */
  setGoogleAssistantEnabled(enabled) {
    this.methodCalled('setGoogleAssistantEnabled', enabled);
  }

  /** @override */
  setGoogleAssistantContextEnabled(enabled) {
    this.methodCalled('setGoogleAssistantContextEnabled', enabled);
  }

  /** @override */
  showGoogleAssistantSettings() {
    this.methodCalled('showGoogleAssistantSettings');
  }
}

suite('GoogleAssistantHandler', function() {

  /** @type {SettingsGoogleAssistantPageElement} */
  var page = null;

  /** @type {?TestGoogleAssistantBrowserProxy} */
  var browserProxy = null;

  setup(function() {
    browserProxy = new TestGoogleAssistantBrowserProxy();
    settings.GoogleAssistantBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    var prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      page = document.createElement('settings-google-assistant-page');
      page.prefs = prefElement.prefs;
      document.body.appendChild(page);
    });
  });

  teardown(function() {
    page.remove();
  });

  test('toggleAssistant', function() {
    Polymer.dom.flush();
    var button = page.$$('#googleAssistantEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    // Tap the enable toggle button and ensure the state becomes enabled.
    MockInteractions.tap(button);
    Polymer.dom.flush();
    assertTrue(button.checked);
    return browserProxy.whenCalled('setGoogleAssistantEnabled')
        .then(assertTrue);
  });

  test('toggleAssistantContext', function() {
    var button = page.$$('#googleAssistantContextEnable');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    Polymer.dom.flush();
    button = page.$$('#googleAssistantContextEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    MockInteractions.tap(button);
    Polymer.dom.flush();
    assertTrue(button.checked);
    return browserProxy.whenCalled('setGoogleAssistantContextEnabled')
        .then(assertTrue);
  });

  test('tapOnAssistantSettings', function() {
    var button = page.$$('#googleAssistantSettings');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    Polymer.dom.flush();
    button = page.$$('#googleAssistantSettings');
    assertTrue(!!button);

    MockInteractions.tap(button);
    Polymer.dom.flush();
    return browserProxy.whenCalled('showGoogleAssistantSettings');
  });

});
