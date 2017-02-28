// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-downloads-page prefs="{{prefs}}">
 *      </settings-downloads-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-downloads-page',

  behaviors: [WebUIListenerBehavior],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Dictionary defining page visibility.
     * @type {!DownloadsPageVisibility}
     */
    pageVisibility: Object,

    /** @private */
    autoOpenDownloads_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?settings.DownloadsBrowserProxy} */
  browserProxy_: null,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.DownloadsBrowserProxyImpl.getInstance();

    this.addWebUIListener('auto-open-downloads-changed', function(autoOpen) {
      this.autoOpenDownloads_ = autoOpen;
    }.bind(this));

    this.browserProxy_.initializeDownloads();
  },

  /** @private */
  selectDownloadLocation_: function() {
    this.browserProxy_.selectDownloadLocation();
  },

  // <if expr="chromeos">
  /**
   * @param {string} path
   * @return {string} The download location string that is suitable to display
   *     in the UI.
   * @private
   */
  getDownloadLocation_: function(path) {
    // Replace /special/drive-<hash>/root with "Google Drive" for remote files,
    // /home/chronos/user/Downloads or /home/chronos/u-<hash>/Downloads with
    // "Downloads" for local paths, and '/' with ' \u203a ' (angled quote sign)
    // everywhere. It is used only for display purpose.
    path = path.replace(/^\/special\/drive[^\/]*\/root/, 'Google Drive');
    path = path.replace(/^\/home\/chronos\/(user|u-[^\/]*)\//, '');
    path = path.replace(/\//g, ' \u203a ');
    return path;
  },
  // </if>

  /** @private */
  onClearAutoOpenFileTypesTap_: function() {
    this.browserProxy_.resetAutoOpenFileTypes();
  },
});
