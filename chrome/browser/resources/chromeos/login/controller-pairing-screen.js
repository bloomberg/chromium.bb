// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('pairing-device-list', (function() {
  /** @const */ var ICON_COLORS = ['#F0B9CB', '#F0ACC3', '#F098B6', '#F084A9',
                                   '#F06D99', '#F05287', '#F0467F', '#F03473',
                                   '#F01E65', '#F00051'];
  return {
    /* Returns pseudo-random color depending of hash of the |name|. */
    colorByName: function(name) {
      var hash = 0;
      for (var i = 0; i < name.length; ++i)
        hash = (name.charCodeAt(i) + 31 * hash) | 0;
      return ICON_COLORS[hash % ICON_COLORS.length];
    }
  };
})());

Polymer('controller-pairing-screen', (function() {
  'use strict';

  // Keep these constants synced with corresponding constants defined in
  // controller_pairing_screen_actor.{h,cc}.
  /** @const */ var CONTEXT_KEY_CONTROLS_DISABLED = 'controlsDisabled';
  /** @const */ var CONTEXT_KEY_SELECTED_DEVICE = 'selectedDevice';
  /** @const */ var CONTEXT_KEY_ACCOUNT_ID = 'accountId';

  /** @const */ var ACTION_ENROLL = 'enroll';

  /** @const */ var PAGE_AUTHENTICATION = 'authentication';

  return {
    gaiaHost_: null,
    selectedDevice: null,

    observe: {
      'C.devices': 'deviceListChanged',
      'C.page': 'pageChanged'
    },

    /** @override */
    initialize: function() {
      this.context.set(CONTEXT_KEY_CONTROLS_DISABLED, true);
      this.commitContextChanges();
      this.gaiaHost_ = new cr.login.GaiaAuthHost(this.$.gaiaFrame);
    },

    pageChanged: function(oldPage, newPage) {
      if (newPage == PAGE_AUTHENTICATION) {
        this.gaiaHost_.load(cr.login.GaiaAuthHost.AuthMode.DEFAULT,
                            {},
                            this.onAuthCompleted_.bind(this));
      }
    },

    deviceListChanged: function() {
      this.selectedDevice = this.context.get(CONTEXT_KEY_SELECTED_DEVICE);
    },

    selectedDeviceChanged: function() {
      this.context.set(CONTEXT_KEY_SELECTED_DEVICE,
          this.selectedDevice ? this.selectedDevice : '');
      this.commitContextChanges();
    },

    helpButtonClicked: function() {
      console.error('Help is not implemented yet.');
    },

    onAuthCompleted_: function(credentials) {
      this.context.set(CONTEXT_KEY_ACCOUNT_ID, credentials.email);
      this.commitContextChanges();
      this.send(login.Screen.CALLBACK_USER_ACTED, ACTION_ENROLL);
    }
  };
})());

