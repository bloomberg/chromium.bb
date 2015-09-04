// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-settings-on-startup-page' is a settings page.
 *
 * Example:
 *
 *    <neon-animated-pages>
 *      <cr-settings-on-startup-page prefs="{{prefs}}">
 *      </cr-settings-on-startup-page>
 *      ... other pages ...
 *    </neon-animated-pages>
 *
 * @group Chrome Settings Elements
 * @element cr-settings-on-startup-page
 */
Polymer({
  is: 'cr-settings-on-startup-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Route for the page.
     */
    route: String,

    /**
     * Whether the page is a subpage.
     */
    subpage: {
      type: Boolean,
      value: false,
      readOnly: true,
    },

    /**
     * ID of the page.
     */
    PAGE_ID: {
      type: String,
      value: 'on-startup',
      readOnly: true,
    },

    /**
     * Title for the page header and navigation menu.
     */
    pageTitle: {
      type: String,
      value: function() {
        return loadTimeData.getString('onStartup');
      },
    },

    /**
     * Name of the 'iron-icon' to show.
     */
    icon: {
      type: String,
      value: 'image:brightness-1',
      readOnly: true,
    },

    inSubpage: {
      type: Boolean,
      notify: true,
    },

    prefValue: {
      type: Number,
    }
  },

  observers: [
    'prefValueChanged(prefValue)',
  ],

  attached: function() {
    this.prefValue = this.prefs.session.restore_on_startup.value;
  },

  prefValueChanged: function(prefValue) {
    this.set('prefs.session.restore_on_startup.value', parseInt(prefValue));
  },

  /** @private */
  onBackTap_: function() {
    this.$.pages.back();
  },

  /** @private */
  onSetPagesTap_: function() {
    this.$.pages.navigateTo('startup-urls');
  },
});
