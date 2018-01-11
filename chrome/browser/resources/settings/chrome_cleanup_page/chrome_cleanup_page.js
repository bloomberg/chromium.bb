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
  REPORTER_FOUND_NOTHING: 'reporter_found_nothing',
  REPORTER_FAILED: 'reporter_failed',
  SCANNING_FOUND_NOTHING: 'scanning_found_nothing',
  SCANNING_FAILED: 'scanning_failed',
  CONNECTION_LOST: 'connection_lost',
  USER_DECLINED_CLEANUP: 'user_declined_cleanup',
  CLEANING_FAILED: 'cleaning_failed',
  CLEANING_SUCCEEDED: 'cleaning_succeeded',
  CLEANER_DOWNLOAD_FAILED: 'cleaner_download_failed',
};

/**
 * The source of the dismiss action. Used when reporting metrics about how the
 * card was dismissed. The numeric values must be kept in sync with the
 * definition of ChromeCleanerDismissSource in
 * src/chrome/browser/ui/webui/settings/chrome_cleanup_handler.cc.
 * @enum {number}
 */
settings.ChromeCleanupDismissSource = {
  OTHER: 0,
  CLEANUP_SUCCESS_DONE_BUTTON: 1,
  CLEANUP_FAILURE_DONE_BUTTON: 2,
};

/**
 * The possible states for the cleanup card.
 * @enum {string}
 */
settings.ChromeCleanerCardState = {
  HIDDEN_CARD: 'hidden',
  SCANNING_OFFERED: 'scanning_offered',
  SCANNING: 'scanning',
  CLEANUP_OFFERED: 'cleanup_offered',
  CLEANING: 'cleaning',
  REBOOT_REQUIRED: 'reboot_required',
  SCANNING_FOUND_NOTHING: 'scanning_found_nothing',
  SCANNING_FAILED: 'scanning_failed',
  CLEANUP_SUCCEEDED: 'cleanup_succeeded',
  CLEANING_FAILED: 'cleanup_failed',
  CLEANER_DOWNLOAD_FAILED: 'cleaner_download_failed',
};

/**
 * Boolean properties for a cleanup card state.
 * @enum {number}
 */
settings.ChromeCleanupCardFlags = {
  NONE: 0,
  SHOW_LOGS_PERMISSIONS: 1 << 0,
  SHOW_LEARN_MORE: 1 << 1,
  WAITING_FOR_RESULT: 1 << 2,
  SHOW_ITEMS_TO_REMOVE: 1 << 3,
};

/**
 * Identifies an ongoing scanning/cleanup action.
 * @enum {number}
 */
settings.ChromeCleanupOngoingAction = {
  NONE: 0,
  SCANNING: 1,
  CLEANING: 2,
};

/**
 * @typedef {{
 *   statusIcon: string,
 *   statusIconClassName: string,
 * }}
 */
settings.ChromeCleanupCardIcon;

/**
 * @typedef {{
 *   label: string,
 *   doAction: !function(),
 * }}
 */
settings.ChromeCleanupCardActionButton;

/**
 * @typedef {{
 *   title: ?string,
 *   explanation: ?string,
 *   icon: ?settings.ChromeCleanupCardIcon,
 *   actionButton: ?settings.ChromeCleanupCardActionButton,
 *   flags: number,
 * }}
 */
settings.ChromeCleanupCardComponents;

/**
 * @typedef {{
 *   files: Array<string>,
 *   registryKeys: Array<string>,
 * }}
 */
settings.ChromeCleanerScannerResults;

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
    userInitiatedCleanupsEnabled_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    title_: {
      type: String,
      value: '',
    },

    /** @private */
    explanation_: {
      type: String,
      value: '',
    },

    /** @private */
    isWaitingForResult_: {
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
    showExplanation_: {
      type: Boolean,
      computed: 'computeShowExplanation_(explanation_)',
    },

    /**
     * Learn more should only be visible for the infected, cleaning and error
     * states.
     * @private
     */
    showLearnMore_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showLogsPermission_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showItemsToRemove_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    itemsToRemoveSectionExpanded_: {
      type: Boolean,
      value: false,
      observer: 'itemsToRemoveSectionExpandedChanged_',
    },

    /** @private */
    showItemsLinkLabel_: {
      type: String,
      value: '',
    },

    /** @private */
    showingAllFiles_: {
      type: Boolean,
      value: false,
    },

    /** @private {!settings.ChromeCleanerScannerResults} */
    scannerResults_: {
      type: Array,
      value: function() {
        return {'files': [], 'registryKeys': []};
      },
    },

    /** @private */
    hasFilesToShow_: {
      type: Boolean,
      computed: 'computeHasFilesToShow_(scannerResults_)',
    },

    /** @private */
    hasRegistryKeysToShow_: {
      type: Boolean,
      computed: 'computeHasRegistryKeysToShow_(' +
          'userInitiatedCleanupsEnabled_, scannerResults_)',
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

    /** @private {chrome.settingsPrivate.PrefObject} */
    logsUploadPref_: {
      type: Object,
      value: function() {
        return /** @type {chrome.settingsPrivate.PrefObject} */ ({});
      },
    },

    /** @private */
    isPoweredByPartner_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {!settings.ChromeCleanerScannerResults} */
  emptyChromeCleanerScannerResults_: {'files': [], 'registryKeys': []},

  /** @private {?settings.ChromeCleanupProxy} */
  browserProxy_: null,

  /** @private {?function()} */
  doAction_: null,

  /** @private {?Map<settings.ChromeCleanerCardState,
   *                 !settings.ChromeCleanupCardComponents>} */
  cardStateToComponentsMap_: null,

  /** @private {settings.ChromeCleanupOngoingAction} */
  ongoingAction_: settings.ChromeCleanupOngoingAction.NONE,

  /**
   * If true, the scan offered view is rendered on state idle, regardless of
   * the idle reason received from the cleaner controller. The goal is to
   * ignore previous interactions (such as completed cleanups) performed on
   * other tabs or if this tab is reloaded.
   * Set to false whenever there is a transition to a non-idle state while the
   * current tab is open.
   * @private {boolean}
   */
  renderScanOfferedByDefault_: true,

  /** @override */
  attached: function() {
    this.userInitiatedCleanupsEnabled_ =
        loadTimeData.getBoolean('userInitiatedCleanupsEnabled');
    this.browserProxy_ = settings.ChromeCleanupProxyImpl.getInstance();
    this.cardStateToComponentsMap_ = this.buildCardStateToComponentsMap_();

    this.addWebUIListener('chrome-cleanup-on-idle', this.onIdle_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-scanning', this.onScanning_.bind(this));
    // Note: both reporter running and scanning share the same UI.
    this.addWebUIListener(
        'chrome-cleanup-on-reporter-running', this.onScanning_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-infected', this.onInfected_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-cleaning', this.onCleaning_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-reboot-required', this.onRebootRequired_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-on-dismiss', this.onDismiss_.bind(this));
    this.addWebUIListener(
        'chrome-cleanup-upload-permission-change',
        this.onUploadPermissionChange_.bind(this));
    this.browserProxy_.registerChromeCleanerObserver();
  },

  /**
   * Implements the action for the only visible button in the UI, which can be
   * either to start an action such as a cleanup or to restart the computer.
   * @private
   */
  proceed_: function() {
    listenOnce(this, 'transitionend', this.doAction_.bind(this));
  },

  getTopSettingsBoxClass_: function(showDetails) {
    return showDetails ? 'top-aligned-settings-box' : 'two-line';
  },

  /**
   * Returns the logs upload permission explanation to be displayed.
   * @param {string} label
   * @return {string}
   * @private
   */
  getLogsPermissionSubLabel_: function(label) {
    return this.userInitiatedCleanupsEnabled_ ? label : '';
  },

  /**
   * Toggles the expand button within the element being listened to.
   * @param {!Event} e
   * @private
   */
  toggleExpandButton_: function(e) {
    // The expand button handles toggling itself.
    const expandButtonTag = 'CR-EXPAND-BUTTON';
    if (e.target.tagName == expandButtonTag)
      return;

    /** @type {!CrExpandButtonElement} */
    const expandButton = e.currentTarget.querySelector(expandButtonTag);
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
  },

  /**
   * Notifies Chrome that the details section was opened or closed.
   * @private
   */
  itemsToRemoveSectionExpandedChanged_: function(newVal, oldVal) {
    if (!oldVal && newVal)
      this.browserProxy_.notifyShowDetails(this.itemsToRemoveSectionExpanded_);
  },

  /**
   * Notifies Chrome that the "learn more" link was clicked.
   * @private
   */
  learnMore_: function() {
    this.browserProxy_.notifyLearnMoreClicked();
  },

  /**
   * @param {string} explanation
   * @return {boolean}
   * @private
   */
  computeShowExplanation_: function(explanation) {
    return explanation != '';
  },

  /**
   * Returns true if there are files to show to the user.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @return {boolean}
   * @private
   */
  computeHasFilesToShow_(scannerResults) {
    return scannerResults.files.length > 0;
  },

  /**
   * Returns true if user-initiated cleanups are enabled and there are registry
   * keys to show to the user.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @return {boolean}
   * @private
   */
  computeHasRegistryKeysToShow_(userInitiatedCleanupsEnabled, scannerResults) {
    return userInitiatedCleanupsEnabled &&
        scannerResults.registryKeys.length > 0;
  },

  /**
   * Listener of event 'chrome-cleanup-on-idle'.
   * @param {string} idleReason
   * @private
   */
  onIdle_: function(idleReason) {
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.NONE;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;

    // If user-initiated cleanups are disabled, then the card will be shown at
    // the top of the settings page.
    if (!this.userInitiatedCleanupsEnabled_) {
      if (idleReason == settings.ChromeCleanupIdleReason.INITIAL) {
        this.dismiss_(settings.ChromeCleanupDismissSource.OTHER);
        return;
      }

      // When user-initiated cleanups are disabled, scanning-related idle
      // reasons are not expected. Show an error message for all reasons other
      // than |CLEANING_SUCCEEDED| and |INITIAL|.
      this.renderCleanupCard_(
          idleReason == settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED ?
              settings.ChromeCleanerCardState.CLEANUP_SUCCEEDED :
              settings.ChromeCleanerCardState.CLEANING_FAILED);
      return;
    }

    // Ignore the idle reason and render the scan offered view if no
    // interaction happened on this tab.
    if (this.renderScanOfferedByDefault_) {
      idleReason = settings.ChromeCleanupIdleReason.INITIAL;
    }

    switch (idleReason) {
      case settings.ChromeCleanupIdleReason.INITIAL:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.SCANNING_OFFERED);
        break;

      case settings.ChromeCleanupIdleReason.SCANNING_FOUND_NOTHING:
      case settings.ChromeCleanupIdleReason.REPORTER_FOUND_NOTHING:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.SCANNING_FOUND_NOTHING);
        break;

      case settings.ChromeCleanupIdleReason.SCANNING_FAILED:
      case settings.ChromeCleanupIdleReason.REPORTER_FAILED:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.SCANNING_FAILED);
        break;

      case settings.ChromeCleanupIdleReason.CONNECTION_LOST:
        if (this.ongoingAction_ ==
            settings.ChromeCleanupOngoingAction.SCANNING) {
          this.renderCleanupCard_(
              settings.ChromeCleanerCardState.SCANNING_FAILED);
        } else {
          assert(
              this.ongoingAction_ ==
              settings.ChromeCleanupOngoingAction.CLEANING);
          this.renderCleanupCard_(
              settings.ChromeCleanerCardState.CLEANING_FAILED);
        }
        break;

      case settings.ChromeCleanupIdleReason.CLEANING_FAILED:
      case settings.ChromeCleanupIdleReason.USER_DECLINED_CLEANUP:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.CLEANING_FAILED);
        break;

      case settings.ChromeCleanupIdleReason.CLEANING_SUCCEEDED:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.CLEANUP_SUCCEEDED);
        break;

      case settings.ChromeCleanupIdleReason.CLEANER_DOWNLOAD_FAILED:
        this.renderCleanupCard_(
            settings.ChromeCleanerCardState.CLEANER_DOWNLOAD_FAILED);
        break;

      default:
        assert(false, `Unknown idle reason: ${idleReason}`);
    }
  },

  /**
   * Listener of event 'chrome-cleanup-on-scanning'.
   * No UI will be shown in the Settings page on that state, simply hide the
   * card and cleanup this element's fields.
   * @private
   */
  onScanning_: function() {
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.SCANNING;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;
    this.renderScanOfferedByDefault_ = false;
    this.renderCleanupCard_(
        this.userInitiatedCleanupsEnabled_ ?
            settings.ChromeCleanerCardState.SCANNING :
            settings.ChromeCleanerCardState.HIDDEN_CARD);
  },

  /**
   * Listener of event 'chrome-cleanup-on-infected'.
   * Offers a cleanup to the user and enables presenting files to be removed.
   * @param {boolean} isPoweredByPartner If scanning results are provided by a
   *     partner's engine.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @private
   */
  onInfected_: function(isPoweredByPartner, scannerResults) {
    this.isPoweredByPartner_ = isPoweredByPartner;
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.NONE;
    this.renderScanOfferedByDefault_ = false;
    this.scannerResults_ = scannerResults;
    this.updateShowItemsLinklabel_();
    this.renderCleanupCard_(settings.ChromeCleanerCardState.CLEANUP_OFFERED);
  },

  /**
   * Listener of event 'chrome-cleanup-on-cleaning'.
   * Shows a spinner indicating that an on-going action and enables presenting
   * files to be removed.
   * @param {boolean} isPoweredByPartner If scanning results are provided by a
   *     partner's engine.
   * @param {!settings.ChromeCleanerScannerResults} scannerResults The cleanup
   *     items to be presented to the user.
   * @private
   */
  onCleaning_: function(isPoweredByPartner, scannerResults) {
    this.isPoweredByPartner_ = isPoweredByPartner;
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.CLEANING;
    this.renderScanOfferedByDefault_ = false;
    this.scannerResults_ = scannerResults;
    this.updateShowItemsLinklabel_();
    this.renderCleanupCard_(settings.ChromeCleanerCardState.CLEANING);
  },

  /**
   * Listener of event 'chrome-cleanup-on-reboot-required'.
   * No UI will be shown in the Settings page on that state, so we simply hide
   * the card and cleanup this element's fields.
   * @private
   */
  onRebootRequired_: function() {
    this.ongoingAction_ = settings.ChromeCleanupOngoingAction.NONE;
    this.scannerResults_ = this.emptyChromeCleanerScannerResults_;
    this.renderScanOfferedByDefault_ = false;
    this.renderCleanupCard_(settings.ChromeCleanerCardState.REBOOT_REQUIRED);
  },

  /**
   * Renders the cleanup card given the state and list of files.
   * @param {!settings.ChromeCleanerCardState} state The card state to be
   *     rendered.
   * @private
   */
  renderCleanupCard_: function(state) {
    const components = this.cardStateToComponentsMap_.get(state);
    assert(components);

    this.title_ = components.title || '';
    this.explanation_ = components.explanation || '';
    this.updateIcon_(components.icon);
    this.updateActionButton_(components.actionButton);
    this.updateCardFlags_(components.flags);
  },

  /**
   * Updates the icon on the cleanup card to show the current state.
   * @param {?settings.ChromeCleanupCardIcon} icon The icon to
   *     render, or null if no icon should be shown.
   * @private
   */
  updateIcon_: function(icon) {
    if (!icon) {
      this.statusIcon_ = '';
      this.statusIconClassName_ = '';
    } else {
      this.statusIcon_ = icon.statusIcon;
      this.statusIconClassName_ = icon.statusIconClassName;
    }
  },

  /**
   * Updates the action button on the cleanup card as the action expected for
   * the current state.
   * @param {?settings.ChromeCleanupCardActionButton} actionButton
   *     The button to render, or null if no button should be shown.
   * @private
   */
  updateActionButton_: function(actionButton) {
    if (!actionButton) {
      this.showActionButton_ = false;
      this.actionButtonLabel_ = '';
      this.doAction_ = null;
    } else {
      this.showActionButton_ = true;
      this.actionButtonLabel_ = actionButton.label;
      this.doAction_ = actionButton.doAction;
    }
  },

  /**
   * Updates boolean flags corresponding to optional components to be rendered
   * on the card.
   * @param {number} flags Flags indicating optional components to be rendered.
   * @private
   */
  updateCardFlags_: function(flags) {
    this.showLogsPermission_ =
        (flags & settings.ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS) != 0;
    this.showLearnMore_ =
        (flags & settings.ChromeCleanupCardFlags.SHOW_LEARN_MORE) != 0;
    this.isWaitingForResult_ =
        (flags & settings.ChromeCleanupCardFlags.WAITING_FOR_RESULT) != 0;
    this.showItemsToRemove_ =
        (flags & settings.ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE) != 0;

    // Files to remove list should only be expandable if details are being
    // shown, otherwise it will add extra padding at the bottom of the card.
    if (!this.showExplanation_ || !this.showItemsToRemove_)
      this.itemsToRemoveSectionExpanded_ = false;
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
   * @param {boolean} enabled Whether logs upload is enabled.
   * @private
   */
  onUploadPermissionChange_: function(enabled) {
    this.logsUploadPref_ = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: enabled,
    };
  },

  /** @private */
  changeLogsPermission_: function() {
    const enabled = this.$.chromeCleanupLogsUploadControl.checked;
    this.browserProxy_.setLogsUploadPermission(enabled);
  },

  /**
   * Dismiss the card.
   * @param {settings.ChromeCleanupDismissSource} source
   * @private
   */
  dismiss_: function(source) {
    // If user initiated cleanups are enabled, the card won't be shown at the
    // top of the settings page and the dismiss button should never be rendered.
    assert(!this.userInitiatedCleanupsEnabled_);

    this.renderCleanupCard_(settings.ChromeCleanerCardState.HIDDEN_CARD);
    this.browserProxy_.dismissCleanupPage(source);
  },

  /**
   * Sends an action to the browser proxy to start scanning.
   * @private
   */
  startScanning_: function() {
    assert(this.userInitiatedCleanupsEnabled_);

    this.browserProxy_.startScanning(
        this.$.chromeCleanupLogsUploadControl.checked);
  },

  /**
   * Sends an action to the browser proxy to start the cleanup.
   * @private
   */
  startCleanup_: function() {
    this.browserProxy_.startCleanup(
        this.$.chromeCleanupLogsUploadControl.checked);
  },

  /**
   * Sends an action to the browser proxy to restart the machine.
   * @private
   */
  restartComputer_: function() {
    this.browserProxy_.restartComputer();
  },

  /**
   * Updates the label for the collapsed detailed view. If user-initiated
   * cleanups are enabled, the string is obtained from the browser proxy, since
   * it may require a plural version. Otherwise, use the default value for
   * |chromeCleanupLinkShowItems|.
   */
  updateShowItemsLinklabel_: function() {
    const setShowItemsLabel = text => this.showItemsLinkLabel_ = text;
    if (this.userInitiatedCleanupsEnabled_) {
      this.browserProxy_
          .getItemsToRemovePluralString(
              this.scannerResults_.files.length +
              this.scannerResults_.registryKeys.length)
          .then(setShowItemsLabel);
    } else {
      setShowItemsLabel(this.i18n('chromeCleanupLinkShowItems'));
    }
  },

  /**
   * Returns the map of card states to components to be rendered.
   * @return {!Map<settings.ChromeCleanerCardState,
   *               !settings.ChromeCleanupCardComponents>}
   * @private
   */
  buildCardStateToComponentsMap_: function() {
    /**
     * The icons to show on the card.
     * @enum {settings.ChromeCleanupCardIcon}
     */
    const icons = {
      // Card's icon indicates a cleanup offer.
      SYSTEM: {
        statusIcon: 'settings:security',
        statusIconClassName: 'status-icon-remove',
      },

      // Card's icon indicates a warning (in case of failure).
      WARNING: {
        statusIcon: 'settings:error',
        statusIconClassName: 'status-icon-warning',
      },

      // Card's icon indicates completion or reboot required.
      DONE: {
        statusIcon: 'settings:check-circle',
        statusIconClassName: 'status-icon-done',
      },
    };

    /**
     * The action buttons to show on the card.
     * @enum {settings.ChromeCleanupCardActionButton}
     */
    const actionButtons = {
      FIND: {
        label: this.i18n('chromeCleanupFindButtonLable'),
        doAction: this.startScanning_.bind(this),
      },

      REMOVE: {
        label: this.i18n('chromeCleanupRemoveButtonLabel'),
        doAction: this.startCleanup_.bind(this),
      },

      RESTART_COMPUTER: {
        label: this.i18n('chromeCleanupRestartButtonLabel'),
        doAction: this.restartComputer_.bind(this),
      },

      DISMISS_CLEANUP_SUCCESS: {
        label: this.i18n('chromeCleanupDoneButtonLabel'),
        doAction: this.dismiss_.bind(
            this,
            settings.ChromeCleanupDismissSource.CLEANUP_SUCCESS_DONE_BUTTON),
      },

      DISMISS_CLEANUP_FAILURE: {
        label: this.i18n('chromeCleanupDoneButtonLabel'),
        doAction: this.dismiss_.bind(
            this,
            settings.ChromeCleanupDismissSource.CLEANUP_FAILURE_DONE_BUTTON),
      },

      TRY_SCAN_AGAIN: {
        label: this.i18n('chromeCleanupTitleTryAgainButtonLabel'),
        // TODO(crbug.com/776538): do not run the reporter component again.
        // Try downloading the cleaner and scan with it instead.
        doAction: this.startScanning_.bind(this),
      },
    };

    // If user-initiated cleanups are enabled, there is no need for a custom
    // link to the Help Center article, as all settings page sections contain
    // a help link by default.
    const learnMoreIfUserInitiatedCleanupsDisabled =
        this.userInitiatedCleanupsEnabled_ ?
        0 :
        settings.ChromeCleanupCardFlags.SHOW_LEARN_MORE;

    return new Map([
      [
        settings.ChromeCleanerCardState.HIDDEN_CARD, {
          title: null,
          icon: null,
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANUP_OFFERED, {
          title: this.i18n('chromeCleanupTitleRemove'),
          explanation: this.i18n('chromeCleanupExplanationRemove'),
          icon: icons.SYSTEM,
          actionButton: actionButtons.REMOVE,
          flags: settings.ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS |
              settings.ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE |
              learnMoreIfUserInitiatedCleanupsDisabled,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANING, {
          title: this.i18n('chromeCleanupTitleRemoving'),
          explanation: this.i18n('chromeCleanupExplanationRemove'),
          icon: null,
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.WAITING_FOR_RESULT |
              learnMoreIfUserInitiatedCleanupsDisabled |
              settings.ChromeCleanupCardFlags.SHOW_ITEMS_TO_REMOVE,
        }
      ],
      [
        settings.ChromeCleanerCardState.REBOOT_REQUIRED, {
          title: this.i18n('chromeCleanupTitleRestart'),
          explanation: null,
          icon: icons.DONE,
          actionButton: actionButtons.RESTART_COMPUTER,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANUP_SUCCEEDED, {
          title: this.i18n('chromeCleanupTitleRemoved'),
          explanation: null,
          icon: icons.DONE,
          actionButton: this.userInitiatedCleanupsEnabled_ ?
              null :
              actionButtons.DISMISS_CLEANUP_SUCCESS,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANING_FAILED, {
          title: this.i18n('chromeCleanupTitleErrorCantRemove'),
          explanation: this.i18n('chromeCleanupExplanationCleanupError'),
          icon: icons.WARNING,
          actionButton: this.userInitiatedCleanupsEnabled_ ?
              null :
              actionButtons.DISMISS_CLEANUP_FAILURE,
          flags: learnMoreIfUserInitiatedCleanupsDisabled,
        }
      ],
      [
        settings.ChromeCleanerCardState.SCANNING_OFFERED, {
          title: this.i18n('chromeCleanupTitleFindAndRemove'),
          explanation: this.i18n('chromeCleanupExplanationFindAndRemove'),
          icon: icons.SYSTEM,
          actionButton: actionButtons.FIND,
          flags: settings.ChromeCleanupCardFlags.SHOW_LOGS_PERMISSIONS,
        }
      ],
      [
        settings.ChromeCleanerCardState.SCANNING, {
          title: this.i18n('chromeCleanupTitleScanning'),
          explanation: null,
          icon: null,
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.WAITING_FOR_RESULT,
        }
      ],
      [
        // TODO(crbug.com/776538): Could we offer to reset settings here?
        settings.ChromeCleanerCardState.SCANNING_FOUND_NOTHING, {
          title: this.i18n('chromeCleanupTitleNothingFound'),
          explanation: null,
          icon: icons.DONE,
          actionButton: null,
          flags: settings.ChromeCleanupCardFlags.NONE,
        }
      ],
      [
        settings.ChromeCleanerCardState.SCANNING_FAILED, {
          title: this.i18n('chromeCleanupTitleScanningFailed'),
          explanation: this.i18n('chromeCleanupExplanationScanError'),
          icon: icons.WARNING,
          actionButton: null,
          flags: learnMoreIfUserInitiatedCleanupsDisabled,
        }
      ],
      [
        settings.ChromeCleanerCardState.CLEANER_DOWNLOAD_FAILED, {
          // TODO(crbug.com/776538): distinguish between missing network
          // connectivity and cleanups being disabled by the server.
          title: this.i18n('chromeCleanupTitleCleanupUnavailable'),
          explanation: this.i18n('chromeCleanupExplanationCleanupUnavailable'),
          icon: icons.WARNING,
          actionButton: actionButtons.TRY_SCAN_AGAIN,
          flags: learnMoreIfUserInitiatedCleanupsDisabled,
        },
      ],
    ]);
  },
});
