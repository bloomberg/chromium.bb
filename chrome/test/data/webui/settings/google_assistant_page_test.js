// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @implements {settings.GoogleAssistantBrowserProxy}
 */
class TestGoogleAssistantBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'showGoogleAssistantSettings',
      'retrainAssistantVoiceModel',
    ]);
  }

  /** @override */
  showGoogleAssistantSettings() {
    this.methodCalled('showGoogleAssistantSettings');
  }

  /** @override */
  retrainAssistantVoiceModel() {
    this.methodCalled('retrainAssistantVoiceModel');
  }
}

suite('GoogleAssistantHandler', function() {

  /** @type {SettingsGoogleAssistantPageElement} */
  let page = null;

  /** @type {?TestGoogleAssistantBrowserProxy} */
  let browserProxy = null;

  suiteSetup(function() {
    loadTimeData.overrideValues({
      enableAssistant: true,
      voiceMatchEnabled: true,
    });
  });

  setup(function() {
    browserProxy = new TestGoogleAssistantBrowserProxy();
    settings.GoogleAssistantBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();

    loadTimeData.overrideValues({
      enableAssistant: true,
    });

    const prefElement = document.createElement('settings-prefs');
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
    const button = page.$$('#googleAssistantEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    // Tap the enable toggle button and ensure the state becomes enabled.
    button.click();
    Polymer.dom.flush();
    assertTrue(button.checked);
  });

  test('toggleAssistantContext', function() {
    let button = page.$$('#googleAssistantContextEnable');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.context.enabled', false);
    Polymer.dom.flush();
    button = page.$$('#googleAssistantContextEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    Polymer.dom.flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.context.enabled.value'), true);
  });

  test('toggleAssistantHotword', function() {
    let button = page.$$('#googleAssistantHotwordEnable');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    Polymer.dom.flush();
    button = page.$$('#googleAssistantHotwordEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    Polymer.dom.flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.hotword.enabled.value'), true);
  });

  test('tapOnRetrainVoiceModel', function() {
    let button = page.$$('#retrainVoiceModel');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue(
        'settings.voice_interaction.activity_control.consent_status',
        ConsentStatus.kActivityControlAccepted);
    Polymer.dom.flush();
    button = page.$$('#retrainVoiceModel');
    assertTrue(!!button);

    button.click();
    Polymer.dom.flush();
    return browserProxy.whenCalled('retrainAssistantVoiceModel');
  });

  test('retrainButtonVisibility', function() {
    let button = page.$$('#retrainVoiceModel');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    Polymer.dom.flush();
    button = page.$$('#retrainVoiceModel');
    assertFalse(!!button);

    // Hotword enabled.
    // Activity control consent not granted.
    // Button should not be shown.
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue(
        'settings.voice_interaction.activity_control.consent_status',
        ConsentStatus.kUnauthorized);
    Polymer.dom.flush();
    button = page.$$('#retrainVoiceModel');
    assertFalse(!!button);

    // Hotword disabled.
    // Activity control consent granted.
    // Button should not be shown.
    page.setPrefValue('settings.voice_interaction.hotword.enabled', false);
    page.setPrefValue(
        'settings.voice_interaction.activity_control.consent_status',
        ConsentStatus.kActivityControlAccepted);
    Polymer.dom.flush();
    button = page.$$('#retrainVoiceModel');
    assertFalse(!!button);

    // Hotword enabled.
    // Activity control consent granted.
    // Button should be shown.
    page.setPrefValue('settings.voice_interaction.hotword.enabled', true);
    page.setPrefValue(
        'settings.voice_interaction.activity_control.consent_status',
        ConsentStatus.kActivityControlAccepted);
    Polymer.dom.flush();
    button = page.$$('#retrainVoiceModel');
    assertTrue(!!button);
  });

  test('toggleAssistantNotification', function() {
    let button = page.$$('#googleAssistantNotificationEnable');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.notification.enabled', false);
    Polymer.dom.flush();
    button = page.$$('#googleAssistantNotificationEnable');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    Polymer.dom.flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.notification.enabled.value'),
        true);
  });

  test('toggleAssistantLaunchWithMicOpen', function() {
    let button = page.$$('#googleAssistantLaunchWithMicOpen');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    page.setPrefValue('settings.voice_interaction.launch_with_mic_open', false);
    Polymer.dom.flush();
    button = page.$$('#googleAssistantLaunchWithMicOpen');
    assertTrue(!!button);
    assertFalse(button.disabled);
    assertFalse(button.checked);

    button.click();
    Polymer.dom.flush();
    assertTrue(button.checked);
    assertTrue(
        page.getPref('settings.voice_interaction.launch_with_mic_open.value'),
        true);
  });

  test('tapOnAssistantSettings', function() {
    let button = page.$$('#googleAssistantSettings');
    assertFalse(!!button);
    page.setPrefValue('settings.voice_interaction.enabled', true);
    Polymer.dom.flush();
    button = page.$$('#googleAssistantSettings');
    assertTrue(!!button);

    button.click();
    Polymer.dom.flush();
    return browserProxy.whenCalled('showGoogleAssistantSettings');
  });

  test('dspHotwordDropdownEnabled', function() {
    let dropdown = page.$$('#dspHotwordState');
    assertFalse(!!dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);

    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.RECOMMENDED,
      value: true,
    });

    Polymer.dom.flush();
    dropdown = page.$$('#dspHotwordState');
    assertFalse(dropdown.hasAttribute('disabled'));
  });

  test('dspHotwordDropdownDisabled', function() {
    let dropdown = page.$$('#dspHotwordState');
    assertFalse(!!dropdown);

    page.setPrefValue('settings.voice_interaction.enabled', true);

    page.set('prefs.settings.voice_interaction.hotword.enabled', {
      enforcement: chrome.settingsPrivate.Enforcement.ENFORCED,
      value: true,
    });

    Polymer.dom.flush();
    dropdown = page.$$('#dspHotwordState');
    assertTrue(dropdown.hasAttribute('disabled'));
  });

});
