// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-about-page' contains version and OS related
 * information.
 */

Polymer({
  is: 'settings-about-page',

  behaviors: [WebUIListenerBehavior, MainPageBehavior, I18nBehavior],

  properties: {
    /** @private {?UpdateStatusChangedEvent} */
    currentUpdateStatusEvent_: {
      type: Object,
      value: {message: '', progress: 0, status: UpdateStatus.DISABLED},
    },

    // <if expr="chromeos">
    /** @private */
    hasCheckedForUpdates_: {
      type: Boolean,
      value: false,
    },

    /** @private {!BrowserChannel} */
    currentChannel_: String,

    /** @private {!BrowserChannel} */
    targetChannel_: String,

    /** @private {?RegulatoryInfo} */
    regulatoryInfo_: Object,
    // </if>

    // <if expr="_google_chrome and is_macosx">
    /** @private {!PromoteUpdaterStatus} */
    promoteUpdaterStatus_: Object,
    // </if>

    /** @private {!{obsolete: boolean, endOfLine: boolean}} */
    obsoleteSystemInfo_: {
      type: Object,
      value: function() {
        return {
          obsolete: loadTimeData.getBoolean('aboutObsoleteNowOrSoon'),
          endOfLine: loadTimeData.getBoolean('aboutObsoleteEndOfTheLine'),
        };
      },
    },

    /** @private */
    showUpdateStatus_: Boolean,

    /** @private */
    showButtonContainer_: Boolean,

    /** @private */
    showRelaunch_: Boolean,

    // <if expr="chromeos">
    /** @private */
    showRelaunchAndPowerwash_: {
      type: Boolean,
      computed: 'computeShowRelaunchAndPowerwash_(' +
          'currentUpdateStatusEvent_, targetChannel_, currentChannel_)',
    },

    /** @private */
    showCheckUpdates_: {
      type: Boolean,
      computed: 'computeShowCheckUpdates_(' +
          'currentUpdateStatusEvent_, hasCheckedForUpdates_)',
    },

    /** @private {!Map<string, string>} */
    focusConfig_: {
      type: Object,
      value: function() {
        var map = new Map();
        if (settings.routes.DETAILED_BUILD_INFO) {
          map.set(
              settings.routes.DETAILED_BUILD_INFO.path,
              '#detailed-build-info-trigger');
        }
        return map;
      },
    },

    /** @private */
    showUpdateWarningDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private {!AboutPageUpdateInfo|undefined} */
    updateInfo_: Object,
    // </if>
  },

  observers: [
    // <if expr="not chromeos">
    'updateShowUpdateStatus_(' +
        'obsoleteSystemInfo_, currentUpdateStatusEvent_)',
    'updateShowRelaunch_(currentUpdateStatusEvent_)',
    'updateShowButtonContainer_(showRelaunch_)',
    // </if>

    // <if expr="chromeos">
    'updateShowUpdateStatus_(' +
        'obsoleteSystemInfo_, currentUpdateStatusEvent_,' +
        'hasCheckedForUpdates_)',
    'updateShowRelaunch_(currentUpdateStatusEvent_, targetChannel_,' +
        'currentChannel_)',
    'updateShowButtonContainer_(' +
        'showRelaunch_, showRelaunchAndPowerwash_, showCheckUpdates_)',
    // </if>
  ],


  /** @private {?settings.AboutPageBrowserProxy} */
  aboutBrowserProxy_: null,

  /** @private {?settings.LifetimeBrowserProxy} */
  lifetimeBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.aboutBrowserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();
    this.aboutBrowserProxy_.pageReady();

    this.lifetimeBrowserProxy_ =
        settings.LifetimeBrowserProxyImpl.getInstance();

    // <if expr="chromeos">
    this.addEventListener('target-channel-changed', e => {
      this.targetChannel_ = e.detail;
    });

    this.aboutBrowserProxy_.getChannelInfo().then(info => {
      this.currentChannel_ = info.currentChannel;
      this.targetChannel_ = info.targetChannel;
      this.startListening_();
    });

    this.aboutBrowserProxy_.getRegulatoryInfo().then(info => {
      this.regulatoryInfo_ = info;
    });
    // </if>
    // <if expr="not chromeos">
    this.startListening_();
    // </if>
    if (settings.getQueryParameters().get('checkForUpdate') == 'true') {
      this.onCheckUpdatesTap_();
    }
  },

  /** @private */
  startListening_: function() {
    this.addWebUIListener(
        'update-status-changed', this.onUpdateStatusChanged_.bind(this));
    // <if expr="_google_chrome and is_macosx">
    this.addWebUIListener(
        'promotion-state-changed',
        this.onPromoteUpdaterStatusChanged_.bind(this));
    // </if>
    this.aboutBrowserProxy_.refreshUpdateStatus();
  },

  /**
   * @param {!UpdateStatusChangedEvent} event
   * @private
   */
  onUpdateStatusChanged_: function(event) {
    // <if expr="chromeos">
    if (event.status == UpdateStatus.CHECKING) {
      this.hasCheckedForUpdates_ = true;
    } else if (event.status == UpdateStatus.NEED_PERMISSION_TO_UPDATE) {
      this.showUpdateWarningDialog_ = true;
      this.updateInfo_ = {version: event.version, size: event.size};
    }
    // </if>
    this.currentUpdateStatusEvent_ = event;
  },

  // <if expr="_google_chrome and is_macosx">
  /**
   * @param {!PromoteUpdaterStatus} status
   * @private
   */
  onPromoteUpdaterStatusChanged_: function(status) {
    this.promoteUpdaterStatus_ = status;
  },

  /**
   * If #promoteUpdater isn't disabled, trigger update promotion.
   * @private
   */
  onPromoteUpdaterTap_: function() {
    // This is necessary because #promoteUpdater is not a button, so by default
    // disable doesn't do anything.
    if (this.promoteUpdaterStatus_.disabled)
      return;
    this.aboutBrowserProxy_.promoteUpdater();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onLearnMoreTap_: function(event) {
    // Stop the propagation of events, so that clicking on links inside
    // actionable items won't trigger action.
    event.stopPropagation();
  },
  // </if>

  /** @private */
  onHelpTap_: function() {
    this.aboutBrowserProxy_.openHelpPage();
  },

  /** @private */
  onRelaunchTap_: function() {
    this.lifetimeBrowserProxy_.relaunch();
  },

  /** @private */
  updateShowUpdateStatus_: function() {
    // <if expr="chromeos">
    // Do not show the "updated" status if we haven't checked yet or the update
    // warning dialog is shown to user.
    if (this.currentUpdateStatusEvent_.status == UpdateStatus.UPDATED &&
        (!this.hasCheckedForUpdates_ || this.showUpdateWarningDialog_)) {
      this.showUpdateStatus_ = false;
      return;
    }
    // </if>
    this.showUpdateStatus_ =
        this.currentUpdateStatusEvent_.status != UpdateStatus.DISABLED &&
        !this.obsoleteSystemInfo_.endOfLine;
  },

  /**
   * Hide the button container if all buttons are hidden, otherwise the
   * container displays an unwanted border (see separator class).
   * @private
   */
  updateShowButtonContainer_: function() {
    // <if expr="not chromeos">
    this.showButtonContainer_ = this.showRelaunch_;
    // </if>
    // <if expr="chromeos">
    this.showButtonContainer_ = this.showRelaunch_ ||
        this.showRelaunchAndPowerwash_ || this.showCheckUpdates_;
    // </if>
  },

  /** @private */
  updateShowRelaunch_: function() {
    // <if expr="not chromeos">
    this.showRelaunch_ = this.checkStatus_(UpdateStatus.NEARLY_UPDATED);
    // </if>
    // <if expr="chromeos">
    this.showRelaunch_ = this.checkStatus_(UpdateStatus.NEARLY_UPDATED) &&
        !this.isTargetChannelMoreStable_();
    // </if>
  },

  /**
   * @return {string}
   * @private
   */
  getUpdateStatusMessage_: function() {
    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.NEED_PERMISSION_TO_UPDATE:
        return this.i18nAdvanced('aboutUpgradeCheckStarted');
      case UpdateStatus.NEARLY_UPDATED:
        // <if expr="chromeos">
        if (this.currentChannel_ != this.targetChannel_)
          return this.i18nAdvanced('aboutUpgradeSuccessChannelSwitch');
        // </if>
        return this.i18nAdvanced('aboutUpgradeRelaunch');
      case UpdateStatus.UPDATED:
        return this.i18nAdvanced('aboutUpgradeUpToDate');
      case UpdateStatus.UPDATING:
        assert(typeof this.currentUpdateStatusEvent_.progress == 'number');
        var progressPercent = this.currentUpdateStatusEvent_.progress + '%';

        // <if expr="chromeos">
        if (this.currentChannel_ != this.targetChannel_) {
          return this.i18nAdvanced('aboutUpgradeUpdatingChannelSwitch', {
            substitutions: [
              this.i18nAdvanced(
                  settings.browserChannelToI18nId(this.targetChannel_)),
              progressPercent
            ]
          });
        }
        // </if>
        if (this.currentUpdateStatusEvent_.progress > 0) {
          // NOTE(dbeam): some platforms (i.e. Mac) always send 0% while
          // updating (they don't support incremental upgrade progress). Though
          // it's certainly quite possible to validly end up here with 0% on
          // platforms that support incremental progress, nobody really likes
          // seeing that they're 0% done with something.
          return this.i18nAdvanced('aboutUpgradeUpdatingPercent', {
            substitutions: [progressPercent],
          });
        }
        return this.i18nAdvanced('aboutUpgradeUpdating');
      default:
        function formatMessage(msg) {
          return parseHtmlSubset('<b>' + msg + '</b>', ['br', 'pre'])
              .firstChild.innerHTML;
        }
        var result = '';
        var message = this.currentUpdateStatusEvent_.message;
        if (message)
          result += formatMessage(message);
        var connectMessage = this.currentUpdateStatusEvent_.connectionTypes;
        if (connectMessage)
          result += '<div>' + formatMessage(connectMessage) + '</div>';
        return result;
    }
  },

  /**
   * @return {?string}
   * @private
   */
  getIcon_: function() {
    // If this platform has reached the end of the line, display an error icon
    // and ignore UpdateStatus.
    if (this.obsoleteSystemInfo_.endOfLine)
      return 'settings:error';

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.DISABLED_BY_ADMIN:
        return 'cr20:domain';
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
    if (this.obsoleteSystemInfo_.endOfLine)
      return null;

    switch (this.currentUpdateStatusEvent_.status) {
      case UpdateStatus.CHECKING:
      case UpdateStatus.UPDATING:
        return 'chrome://resources/images/throbber_small.svg';
      default:
        return null;
    }
  },

  /**
   * @param {!UpdateStatus} status
   * @return {boolean}
   * @private
   */
  checkStatus_: function(status) {
    return this.currentUpdateStatusEvent_.status == status;
  },

  // <if expr="chromeos">
  /**
   * @return {boolean}
   * @private
   */
  isTargetChannelMoreStable_: function() {
    assert(this.currentChannel_.length > 0);
    assert(this.targetChannel_.length > 0);
    return settings.isTargetChannelMoreStable(
        this.currentChannel_, this.targetChannel_);
  },

  /** @private */
  onDetailedBuildInfoTap_: function() {
    settings.navigateTo(settings.routes.DETAILED_BUILD_INFO);
  },

  /** @private */
  onRelaunchAndPowerwashTap_: function() {
    this.lifetimeBrowserProxy_.factoryReset();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowRelaunchAndPowerwash_: function() {
    return this.checkStatus_(UpdateStatus.NEARLY_UPDATED) &&
        this.isTargetChannelMoreStable_();
  },

  /** @private */
  onCheckUpdatesTap_: function() {
    this.onUpdateStatusChanged_({status: UpdateStatus.CHECKING});
    this.aboutBrowserProxy_.requestUpdate();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowCheckUpdates_: function() {
    // Enable the update button if we are in a stale 'updated' status or
    // update has failed. Disable it otherwise.
    var staleUpdatedStatus =
        !this.hasCheckedForUpdates_ && this.checkStatus_(UpdateStatus.UPDATED);

    return staleUpdatedStatus || this.checkStatus_(UpdateStatus.FAILED);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowRegulatoryInfo_: function() {
    return this.regulatoryInfo_ !== null;
  },

  /** @private */
  onUpdateWarningDialogClose_: function() {
    this.showUpdateWarningDialog_ = false;
    // Shows 'check for updates' button in case that the user cancels the
    // dialog and then intends to check for update again.
    this.hasCheckedForUpdates_ = false;
  },
  // </if>

  /** @private */
  onProductLogoTap_: function() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  },

  // <if expr="_google_chrome">
  /** @private */
  onReportIssueTap_: function() {
    this.aboutBrowserProxy_.openFeedbackDialog();
  },
  // </if>
});
