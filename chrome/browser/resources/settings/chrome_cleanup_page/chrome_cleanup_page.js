// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The reason why the controller is in state kIdle.
 * Must be kept in sync with ChromeCleanerController::IdleReason.
 * @enum {string}
 */
settings.ChromeCleanupIdleReason = {
  INITIAL: 'initial',
  SCANNING_FOUND_NOTHING: 'scanning_found_nothing',
  SCANNING_FAILED: 'scanning_failed',
  CONNECTION_LOST: 'connection_lost',
  USER_DECLINED_CLEANUP: 'user_declined_cleanup',
  CLEANING_FAILED: 'cleaning_failed',
  CLEANING_SUCCEEDED: 'cleaning_succeeded',
};

/**
 * @fileoverview
 * 'settings-chrome-cleanup-page' is the settings page containing Chrome
 * Cleanup settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-chrome-cleanup-page></settings-chrome-cleanup-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-chrome-cleanup-page',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /** @private */
    title_: {
      type: String,
      value: '',
    },

    /** @private */
    isRemoving_: {
      type: Boolean,
      value: '',
    },

    /** @private */
    showActionButton_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    actionButtonLabel_: {
      type: String,
      value: '',
    },

    /** @private */
    showDetails_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    detailsDescription: {
      type: String,
      value: '',
    },

    /** @private */
    showFilesToRemove_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    filesToRemove_: {
      type: Array,
      value: [],
    },

    /** @private */
    statusIcon_: {
      type: String,
      value: '',
    },

    /** @private */
    statusIconClassName_: {
      type: String,
      value: '',
    },
  },

  /** @private {?settings.ChromeCleanupProxy} */
  browserProxy_: null,

  /** @private {?function()} */
  doAction_: null,

  /**
   * If true, this settings page is waiting for cleanup results.
   * @private {boolean}
   */
  waitingForCleanupResults_: false,

  /** @override */
  attached: function() {
    this.browserProxy_ = settings.ChromeCleanupProxyImpl.getInstance();

    this.addWebUIListener('chrome-cleanup-on-idle', this.onIdle_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-scanning', this.onScanning_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-infected', this.onInfected_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-cleaning', this.onCleaning_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-reboot-required', this.onRebootRequired_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-dismiss', this.onDismiss_.bind(this));

    this.browserProxy_.registerChromeCleanerObserver();
  },

  /**
   * Implements the action for the only visible button in the UI, which can be
   * either to start a cleanup or to restart the computer.
   * @private
   */
  proceed_: function() {
    listenOnce(this, 'transitionend', this.doAction_.bind(this));
  },

  /**
   * Enables presenting the list of files to be removed by Chrome Cleanup.
   * @private
   */
  showFiles_: function() {
    this.showFilesToRemove_ = true;
  },

  /**
   * Listener of event 'chrome-cleanup-on-idle'.
   * @param {number} idleReason
   * @private
   */
  onIdle_: function(idleReason) {
    if (idleReason == settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED) {
      this.title_ = this.i18n('chromeCleanupTitleRemoved');
      this.enableActionButton_(
          this.i18n('chromeCleanupDoneButtonLabel'), this.dismiss_.bind(this));
      this.setIconDone_();
    } else if (idleReason == settings.ChromeCleanupIdleReason.CLEANING_FAILED) {
      this.title_ = this.i18n('chromeCleanupTitleErrorCantRemove');
      this.enableActionButton_(
          this.i18n('chromeCleanupDoneButtonLabel'), this.dismiss_.bind(this));
      this.setIconWarning_();
    } else {
      // TODO(proberge): Handle other cases.
      this.title_ = '';
      this.disableActionButton_();
    }

    this.isRemoving_ = false;
    this.disableDetails_();
    this.waitingForCleanupResults_ = false;
  },

  /**
   * Listener of event 'chrome-cleanup-on-scanning'.
   * No UI will be shown in the Settings page on that state, so we simply hide
   * the card and cleanup this element's fields.
   * @private
   */
  onScanning_: function() {
    this.title_ = '';
    this.isRemoving_ = false;
    this.disableActionButton_();
    this.disableDetails_();
  },

  /**
   * Listener of event 'chrome-cleanup-on-infected'.
   * Offers a cleanup to the user and enables presenting files to be removed.
   * @param {!Array<!string>} files The list of files to present to the user.
   * @private
   */
  onInfected_: function(files) {
    this.title_ = this.i18n('chromeCleanupTitleRemove');
    this.isRemoving_ = false;
    this.setIconRemove_();
    this.enableActionButton_(
        this.i18n('chromeCleanupRemoveButtonLabel'),
        this.startCleanup_.bind(this));
    this.enableDetails_(files);
  },

  /**
   * Listener of event 'chrome-cleanup-on-cleaning'.
   * Shows a spinner indicating that an on-going action and enables presenting
   * files to be removed.
   * @param {!Array<!string>} files The list of files to present to the user.
   * @private
   */
  onCleaning_: function(files) {
    this.waitingForCleanupResults_ = true;
    this.title_ = this.i18n('chromeCleanupTitleRemoving');
    this.isRemoving_ = true;
    this.resetIcon_();
    this.disableActionButton_();
    this.enableDetails_(files);
  },

  /**
   * Listener of event 'chrome-cleanup-on-reboot-required'.
   * No UI will be shown in the Settings page on that state, so we simply hide
   * the card and cleanup this element's fields.
   * @private
   */
  onRebootRequired_: function() {
    this.title_ = this.i18n('chromeCleanupTitleRestart');
    this.isRemoving_ = false;
    this.setIconWarning_();
    this.enableActionButton_(
        this.i18n('chromeCleanupRestartButtonLabel'),
        this.restartComputer_.bind(this));
    this.disableDetails_();
  },

  /**
   * Listener of event 'chrome-cleanup-dismiss'.
   * Hides the Cleanup card.
   * @private
   */
  onDismiss_: function() {
    this.fire('chrome-cleanup-dismissed');
  },

  /**
   * Dismiss the card.
   * @private
   */
  dismiss_: function() {
    this.browserProxy_.dismissCleanupPage();
  },

  /**
   * Hides the action button in the card when no action is available.
   * @private
   */
  disableActionButton_: function() {
    this.showActionButton_ = false;
    this.actionButtonLabel_ = '';
    this.doAction_ = null;
  },

  /**
   * Shows the action button in the card with an associated label and action
   * function.
   * @param {!string} buttonLabel The label for the action button.
   * @param {!function()} action The function associated with the on-tap event.
   * @private
   */
  enableActionButton_: function(buttonLabel, action) {
    this.showActionButton_ = true;
    this.actionButtonLabel_ = buttonLabel;
    this.doAction_ = action;
  },

  /**
   * Disables the details section on the card.
   * @private
   */
  disableDetails_: function() {
    this.showDetails_ = false;
    this.detailsDescription = '';
    this.showFilesToRemove_ = false;
    this.filesToRemove_ = [];
  },

  /**
   * Enables the details section on the card.
   * @param {!Array<!string>} files The list of files to present to the user.
   * @private
   */
  enableDetails_: function(files) {
    this.showDetails_ = true;
    this.detailsDescription = this.i18n('chromeCleanupExplanation');
    // Note: doesn't change the state of this.showFilesToRemove_.
    this.filesToRemove_ = files;
  },

  /**
   * Sends an action to the browser proxy to start the cleanup.
   * @private
   */
  startCleanup_: function() {
    this.browserProxy_.startCleanup();
  },

  /**
   * Sends an action to the browser proxy to restart the machine.
   * @private
   */
  restartComputer_: function() {
    this.browserProxy_.restartComputer();
  },

  /**
   * Sets the card's icon as the cleanup offered indication.
   * @private
   */
  setIconRemove_: function() {
    this.statusIcon_ = 'settings:security';
    this.statusIconClassName_ = 'status-icon-remove';
  },

  /**
   * Sets the card's icon as a warning (in case of computer restart required
   * or failure).
   * @private
   */
  setIconWarning_: function() {
    this.statusIcon_ = 'settings:error';
    this.statusIconClassName_ = 'status-icon-warning';
  },

  /**
   * Sets the card's icon as a completed indication.
   * @private
   */
  setIconDone_: function() {
    this.statusIcon_ = 'settings:check-circle';
    this.statusIconClassName_ = 'status-icon-done';
  },

  /**
   * Resets the card's icon.
   * @private
   */
  resetIcon_: function() {
    this.statusIcon_ = '';
    this.statusIconClassName_ = '';
  },
});
