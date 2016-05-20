// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */
Polymer({
  is: 'settings-about-page',

  behaviors: [WebUIListenerBehavior, RoutableBehavior, I18nBehavior],

  properties: {
    /**
     * The current active route.
     */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /** @private {?UpdateStatusChangedEvent} */
    currentUpdateStatusEvent_: Object,

<if expr="chromeos">
    /**
     * Whether the current and target channel is different.
     * @private
     */
    channelsDiffer_: Boolean,

    /** @private {!BrowserChannel} */
    targetChannel_: String,
</if>
  },

  /** @private {?settings.AboutPageBrowserProxy} */
  browserProxy_: null,

  /**
   * @type {string} Selector to get the sections.
   * TODO(michaelpg): replace duplicate docs with @override once b/24294625
   * is fixed.
   */
  sectionSelector: 'settings-section',

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();
    this.browserProxy_.pageReady();

<if expr="chromeos">
    Promise.all([
      this.browserProxy_.getCurrentChannel(),
      this.browserProxy_.getTargetChannel(),
    ]).then(function(channels) {
      this.targetChannel_ = channels[1];
      this.channelsDiffer_ = channels[0] != this.targetChannel_;
      this.startListening_();
    }.bind(this));
</if>
<if expr="not chromeos">
    this.startListening_();
</if>
  },

  /** @private */
  startListening_: function() {
    this.addWebUIListener(
        'update-status-changed',
        /** @param {!UpdateStatusChangedEvent} event */
        function(event) {
          this.currentUpdateStatusEvent_ = event;
        }.bind(this));
    this.browserProxy_.refreshUpdateStatus();
  },

   /** @override */
  attached: function() {
    this.scroller = this.parentElement;
  },

  /** @private */
  onHelpTap_: function() {
    this.browserProxy_.openHelpPage();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowUpdateStatus_: function() {
    return this.currentUpdateStatusEvent_.status != UpdateStatus.DISABLED;
  },

  /**
   * @return {string}
   * @private
   */
  getUpdateStatusMessage_: function() {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
        return this.i18n('aboutUpgradeCheckStarted');
      case UpdateStatus.NEARLY_UPDATED:
<if expr="chromeos">
        if (this.channelsDiffer_)
          return this.i18n('aboutUpgradeSuccessChannelSwitch');
</if>
        return this.i18n('aboutUpgradeRelaunch');
      case UpdateStatus.UPDATED:
        return this.i18n('aboutUpgradeUpToDate');
      case UpdateStatus.UPDATING:
<if expr="chromeos">
        if (this.channelsDiffer_) {
          return this.i18n('aboutUpgradeUpdatingChannelSwitch',
              this.i18n(settings.browserChannelToI18nId(this.targetChannel_)));
        }
</if>
        return this.i18n('aboutUpgradeUpdating');
      default:
        return this.currentUpdateStatusEvent_.message;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getIcon_: function() {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr:domain';
      case UpdateStatus.FAILED:
        return 'settings:error';
      case UpdateStatus.UPDATED:
      case UpdateStatus.NEARLY_UPDATED:
          return 'settings:check-circle';
      default:
          return null;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getIconSrc_: function() {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

<if expr="chromeos">
  /** @private */
  onDetailedBuildInfoTap_: function() {
    var animatedPages = /** @type {!SettingsAnimatedPagesElement} */ (
        this.$.pages);
    animatedPages.setSubpageChain(['detailed-build-info']);
  },
</if>

<if expr="_google_chrome">
  /** @private */
  onReportIssueTap_: function() {
    this.browserProxy_.openFeedbackDialog();
  },
</if>
});
