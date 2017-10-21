// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  const DetailView = Polymer({
    is: 'extensions-detail-view',

    behaviors: [I18nBehavior, CrContainerShadowBehavior],

    properties: {
      /**
       * The underlying ExtensionInfo for the details being displayed.
       * @type {chrome.developerPrivate.ExtensionInfo}
       */
      data: Object,

      /** @type {!extensions.ItemDelegate} */
      delegate: Object,

      /** Whether the user has enabled the UI's developer mode. */
      inDevMode: Boolean,
    },

    /** @private */
    onCloseButtonTap_: function() {
      extensions.navigation.navigateTo({page: Page.LIST});
    },

    /**
     * @return {boolean}
     * @private
     */
    isControlled_: function() {
      return extensions.isControlled(this.data);
    },

    /**
     * @return {boolean}
     * @private
     */
    isEnabled_: function() {
      return extensions.isEnabled(this.data.state);
    },

    /**
     * @return {boolean}
     * @private
     */
    isEnableToggleEnabled_: function() {
      return extensions.userCanChangeEnablement(this.data);
    },

    /**
     * @return {boolean}
     * @private
     */
    hasDependentExtensions_: function() {
      return this.data.dependentExtensions.length > 0;
    },

    /**
     * @return {boolean}
     * @private
     */
    hasWarnings_: function() {
      return this.data.disableReasons.corruptInstall ||
          this.data.disableReasons.suspiciousInstall ||
          this.data.disableReasons.updateRequired || !!this.data.blacklistText;
    },

    /**
     * @return {string}
     * @private
     */
    computeEnabledStyle_: function() {
      return this.isEnabled_() ? 'enabled-text' : '';
    },

    /**
     * @return {string}
     * @private
     */
    computeEnabledText_: function() {
      // TODO(devlin): Get the full spectrum of these strings from bettes.
      return this.isEnabled_() ? this.i18n('itemOn') : this.i18n('itemOff');
    },

    /**
     * @param {!chrome.developerPrivate.ExtensionView} view
     * @return {string}
     * @private
     */
    computeInspectLabel_: function(view) {
      return extensions.computeInspectableViewLabel(view);
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldShowHomepageButton_: function() {
      // Note: we ignore |data.homePage.specified| - we use an extension's
      // webstore entry as a homepage if the extension didn't explicitly specify
      // a homepage. (|url| can still be unset in the case of unpacked
      // extensions.)
      return this.data.homePage.url.length > 0;
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldShowOptionsLink_: function() {
      return !!this.data.optionsPage;
    },

    /**
     * @return {boolean}
     * @private
     */
    shouldShowOptionsSection_: function() {
      return this.data.incognitoAccess.isEnabled ||
          this.data.fileAccess.isEnabled || this.data.runOnAllUrls.isEnabled ||
          this.data.errorCollection.isEnabled;
    },

    /** @private */
    onEnableChange_: function() {
      this.delegate.setItemEnabled(
          this.data.id, this.$['enable-toggle'].checked);
    },

    /**
     * @param {!{model: !{item: !chrome.developerPrivate.ExtensionView}}} e
     * @private
     */
    onInspectTap_: function(e) {
      this.delegate.inspectItemView(this.data.id, e.model.item);
    },

    /** @private */
    onOptionsTap_: function() {
      this.delegate.showItemOptionsPage(this.data.id);
    },

    /** @private */
    onRemoveTap_: function() {
      this.delegate.deleteItem(this.data.id);
    },

    /** @private */
    onRepairTap_: function() {
      this.delegate.repairItem(this.data.id);
    },

    /** @private */
    onAllowIncognitoChange_: function() {
      this.delegate.setItemAllowedIncognito(
          this.data.id, this.$$('#allow-incognito').checked);
    },

    /** @private */
    onAllowOnFileUrlsChange_: function() {
      this.delegate.setItemAllowedOnFileUrls(
          this.data.id, this.$$('#allow-on-file-urls').checked);
    },

    /** @private */
    onAllowOnAllSitesChange_: function() {
      this.delegate.setItemAllowedOnAllSites(
          this.data.id, this.$$('#allow-on-all-sites').checked);
    },

    /** @private */
    onCollectErrorsChange_: function() {
      this.delegate.setItemCollectsErrors(
          this.data.id, this.$$('#collect-errors').checked);
    },

    /**
     * @param {!chrome.developerPrivate.DependentExtension} item
     * @private
     */
    computeDependentEntry_: function(item) {
      return loadTimeData.getStringF('itemDependentEntry', item.name, item.id);
    },

    /** @private */
    computeSourceString_: function() {
      return extensions.getItemSourceString(
          extensions.getItemSource(this.data));
    },

    /**
     * @param {chrome.developerPrivate.ControllerType} type
     * @return {string}
     * @private
     */
    getIndicatorIcon_: function(type) {
      switch (type) {
        case 'POLICY':
          return 'cr20:domain';
        case 'CHILD_CUSTODIAN':
          return 'cr:account-child-invert';
        case 'SUPERVISED_USER_CUSTODIAN':
          return 'cr:supervisor-account';
        default:
          return '';
      }
    },
  });

  return {DetailView: DetailView};
});
