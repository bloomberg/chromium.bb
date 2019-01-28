// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The types of Hotword enable status without Dsp support.
 * @enum {number}
 */
const DspHotwordState = {
  DEFAULT_ON: 0,
  ALWAYS_ON: 1,
  OFF: 2,
};

/**
 * @fileoverview 'settings-google-assistant-page' is the settings page
 * containing Google Assistant settings.
 */
Polymer({
  is: 'settings-google-assistant-page',

  behaviors: [I18nBehavior, PrefsBehavior],

  properties: {
    /** @private */
    assistantFeatureEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('enableAssistant');
      },
    },

    /** @private */
    voiceMatchEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hotwordDspAvailable_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('hotwordDspAvailable');
      },
    },

    /** @private */
    hotwordDropdownList_: {
      type: Array,
      value: function() {
        return [
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspRecommended'),
            value: DspHotwordState.DEFAULT_ON
          },
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspAlwaysOn'),
            value: DspHotwordState.ALWAYS_ON
          },
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspOff'),
            value: DspHotwordState.OFF
          }
        ];
      },
    },

    /** @private */
    hotwordDefaultOn_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    dspHotwordState_: {
      type: DspHotwordState,
    }
  },

  observers: [
    'onPrefsChanged_(prefs.settings.voice_interaction.hotword.enabled.value)',
    'onPrefsChanged_(prefs.settings.voice_interaction.hotword.always_on.value)',
  ],

  /** @private {?settings.GoogleAssistantBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = settings.GoogleAssistantBrowserProxyImpl.getInstance();
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantOnOffLabel_: function(toggleValue) {
    return this.i18n(toggleValue ? 'toggleOn' : 'toggleOff');
  },

  /** @private */
  onGoogleAssistantSettingsTapped_: function() {
    this.browserProxy_.showGoogleAssistantSettings();
  },

  /** @private */
  onRetrainVoiceModelTapped_: function() {
    this.browserProxy_.retrainAssistantVoiceModel();
  },

  /** @private */
  onDspHotwordStateChange_: function() {
    switch (Number(this.$$('#dspHotwordState').value)) {
      case DspHotwordState.DEFAULT_ON:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', true);
        this.setPrefValue(
            'settings.voice_interaction.hotword.always_on', false);
        this.hotwordDefaultOn_ = true;
        break;
      case DspHotwordState.ALWAYS_ON:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', true);
        this.setPrefValue('settings.voice_interaction.hotword.always_on', true);
        this.hotwordDefaultOn_ = false;
        break;
      case DspHotwordState.OFF:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', false);
        this.setPrefValue(
            'settings.voice_interaction.hotword.always_on', false);
        this.hotwordDefaultOn_ = false;
        break;
      default:
        console.error('Invalid Dsp hotword settings state');
    }
  },

  /**
   * @param {number} state
   * @return {boolean}
   * @private
   */
  isDspHotwordStateMatch_: function(state) {
    return state == this.dspHotwordState_;
  },

  /** @private */
  onPrefsChanged_: function() {
    this.refreshDspHotwordState_();

    this.voiceMatchEnabled_ = loadTimeData.getBoolean('voiceMatchEnabled') &&
        this.getPref('settings.voice_interaction.hotword.enabled.value');
  },

  /** @private */
  refreshDspHotwordState_: function() {
    if (!this.getPref('settings.voice_interaction.hotword.enabled.value')) {
      this.hotwordDefaultOn_ = false;
      this.dspHotwordState_ = DspHotwordState.OFF;
    } else if (this.getPref(
                   'settings.voice_interaction.hotword.always_on.value')) {
      this.hotwordDefaultOn_ = false;
      this.dspHotwordState_ = DspHotwordState.ALWAYS_ON;
    } else {
      this.hotwordDefaultOn_ = true;
      this.dspHotwordState_ = DspHotwordState.DEFAULT_ON;
    }

    if (this.$$('#dspHotwordState')) {
      this.$$('#dspHotwordState').value = this.dspHotwordState_;
    }
  }
});
