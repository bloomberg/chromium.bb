// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="utils.js">
// <include src="setting_zippy.js">
// <include src="voice_match_entry.js">
// <include src="assistant_get_more.js">
// <include src="assistant_loading.js">
// <include src="assistant_third_party.js">
// <include src="assistant_value_prop.js">
// <include src="assistant_voice_match.js">

/**
 * @fileoverview Polymer element for displaying material design assistant
 * ready screen.
 *
 */

Polymer({
  is: 'assistant-optin-flow',

  behaviors: [OobeDialogHostBehavior],

  /**
   * Indicates the type of the opt-in flow.
   */
  FlowType: {
    // The whole consent flow.
    CONSENT_FLOW: 0,
    // The voice match enrollment flow.
    SPEAKER_ID_ENROLLMENT: 1,
  },

  /**
   * Signal from host to show the screen.
   * @param {?number} type The type of the flow.
   */
  onShow: function(type) {
    var flowType = Number(type);
    switch (flowType) {
      case this.FlowType.CONSENT_FLOW:
      case this.FlowType.SPEAKER_ID_ENROLLMENT:
        this.flowType = flowType;
        break;
      default:
        console.error('Invalid flow type, using default.');
        this.flowType = this.FlowType.CONSENT_FLOW;
        break;
    }

    this.boundShowLoadingScreen = this.showLoadingScreen.bind(this);
    this.boundOnScreenLoadingError = this.onScreenLoadingError.bind(this);
    this.boundOnScreenLoaded = this.onScreenLoaded.bind(this);

    this.$['loading'].onBeforeShow();
    this.$['loading'].addEventListener('reload', this.onReload.bind(this));

    switch (this.flowType) {
      case this.FlowType.SPEAKER_ID_ENROLLMENT:
        this.showScreen(this.$['voice-match']);
        break;
      default:
        this.showScreen(this.$['value-prop']);
    }
    chrome.send('login.AssistantOptInFlowScreen.initialized', [this.flowType]);
  },

  /**
   * Reloads localized strings.
   * @param {!Object} data New dictionary with i18n values.
   */
  reloadContent: function(data) {
    this.voiceMatchFeatureEnabled = data['voiceMatchFeatureEnabled'];
    data['flowType'] = this.flowType;
    this.$['value-prop'].reloadContent(data);
    this.$['third-party'].reloadContent(data);
    this.$['get-more'].reloadContent(data);
  },

  /**
   * Add a setting zippy object in the corresponding screen.
   * @param {string} type type of the setting zippy.
   * @param {!Object} data String and url for the setting zippy.
   */
  addSettingZippy: function(type, data) {
    switch (type) {
      case 'settings':
        this.$['value-prop'].addSettingZippy(data);
        break;
      case 'disclosure':
        this.$['third-party'].addSettingZippy(data);
        break;
      case 'get-more':
        this.$['get-more'].addSettingZippy(data);
        break;
      default:
        console.error('Undefined zippy data type: ' + type);
    }
  },

  /**
   * Show the next screen in the flow.
   */
  showNextScreen: function() {
    switch (this.currentScreen) {
      case this.$['value-prop']:
        this.showScreen(this.$['third-party']);
        break;
      case this.$['third-party']:
        if (this.voiceMatchFeatureEnabled) {
          this.showScreen(this.$['voice-match']);
        } else {
          this.showScreen(this.$['get-more']);
        }
        break;
      case this.$['voice-match']:
        if (this.flowType == this.FlowType.SPEAKER_ID_ENROLLMENT) {
          chrome.send('login.AssistantOptInFlowScreen.flowFinished');
        } else {
          this.showScreen(this.$['get-more']);
        }
        break;
      case this.$['get-more']:
        this.showScreen(this.$['ready']);
        break;
      default:
        console.error('Undefined');
        chrome.send('dialogClose');
    }
  },

  /**
   * Called when the Voice match state is updated.
   * @param {string} state the voice match state.
   */
  onVoiceMatchUpdate: function(state) {
    if (!this.currentScreen == this.$['voice-match']) {
      return;
    }
    switch (state) {
      case 'listen':
        this.$['voice-match'].listenForHotword();
        break;
      case 'process':
        this.$['voice-match'].processingHotword();
        break;
      case 'done':
        this.$['voice-match'].voiceMatchDone();
        break;
      default:
        break;
    }
  },

  /**
   * Show the given screen.
   *
   * @param {Element} screen The screen to be shown.
   */
  showScreen: function(screen) {
    if (this.currentScreen == screen) {
      return;
    }

    screen.hidden = false;
    screen.addEventListener('loading', this.boundShowLoadingScreen);
    screen.addEventListener('error', this.boundOnScreenLoadingError);
    screen.addEventListener('loaded', this.boundOnScreenLoaded);
    if (this.currentScreen) {
      this.currentScreen.hidden = true;
      this.currentScreen.removeEventListener(
          'loading', this.boundShowLoadingScreen);
      this.currentScreen.removeEventListener(
          'error', this.boundOnScreenLoadingError);
      this.currentScreen.removeEventListener(
          'loaded', this.boundOnScreenLoaded);
    }
    this.currentScreen = screen;
    this.currentScreen.onBeforeShow();
    this.currentScreen.onShow();
  },

  /**
   * Show the loading screen.
   */
  showLoadingScreen: function() {
    this.$['loading'].hidden = false;
    this.currentScreen.hidden = true;
    this.$['loading'].onShow();
  },

  /**
   * Called when the screen failed to load.
   */
  onScreenLoadingError: function() {
    this.$['loading'].hidden = false;
    this.currentScreen.hidden = true;
    this.$['loading'].onErrorOccurred();
  },

  /**
   * Called when all the content of current screen has been loaded.
   */
  onScreenLoaded: function() {
    this.currentScreen.hidden = false;
    this.$['loading'].hidden = true;
    this.$['loading'].onPageLoaded();
  },

  /**
   * Called when user request the screen to be reloaded.
   */
  onReload: function() {
    this.currentScreen.reloadPage();
  },
});
