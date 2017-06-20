// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  /** @interface */
  var ItemDelegate = function() {};

  ItemDelegate.prototype = {
    /** @param {string} id */
    deleteItem: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isEnabled
     */
    setItemEnabled: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isAllowedIncognito
     */
    setItemAllowedIncognito: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isAllowedOnFileUrls
     */
    setItemAllowedOnFileUrls: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} isAllowedOnAllSites
     */
    setItemAllowedOnAllSites: assertNotReached,

    /**
     * @param {string} id
     * @param {boolean} collectsErrors
     */
    setItemCollectsErrors: assertNotReached,

    /**
     * @param {string} id
     * @param {chrome.developerPrivate.ExtensionView} view
     */
    inspectItemView: assertNotReached,

    /** @param {string} id */
    reloadItem: assertNotReached,

    /** @param {string} id */
    repairItem: assertNotReached,

    /** @param {string} id */
    showItemOptionsPage: assertNotReached,
  };

  var Item = Polymer({
    is: 'extensions-item',

    behaviors: [I18nBehavior],

    properties: {
      // The item's delegate, or null.
      delegate: {
        type: Object,
      },

      // Whether or not dev mode is enabled.
      inDevMode: {
        type: Boolean,
        value: false,
      },

      // The underlying ExtensionInfo itself. Public for use in declarative
      // bindings.
      /** @type {chrome.developerPrivate.ExtensionInfo} */
      data: {
        type: Object,
      },

      // Whether or not the expanded view of the item is shown.
      /** @private */
      showingDetails_: {
        type: Boolean,
        value: false,
      },
    },

    observers: [
      'observeIdVisibility_(inDevMode, showingDetails_, data.id)',
    ],

    /** @private */
    observeIdVisibility_: function(inDevMode, showingDetails, id) {
      Polymer.dom.flush();
      var idElement = this.$$('#extension-id');
      if (idElement) {
        assert(this.data);
        idElement.innerHTML = this.i18n('itemId', this.data.id);
      }
      this.fire('extension-item-size-changed', {item: this.data});
    },

    /**
     * @return {boolean}
     * @private
     */
    computeErrorsHidden_: function() {
      return !this.data.manifestErrors.length &&
          !this.data.runtimeErrors.length;
    },

    /** @private */
    onRemoveTap_: function() {
      this.delegate.deleteItem(this.data.id);
    },

    /** @private */
    onEnableChange_: function() {
      this.delegate.setItemEnabled(
          this.data.id, this.$['enable-toggle'].checked);
    },

    /** @private */
    onErrorsTap_: function() {
      this.fire('extension-item-show-errors', {data: this.data});
    },

    /** @private */
    onDetailsTap_: function() {
      this.fire('extension-item-show-details', {data: this.data});
    },

    /**
     * @param {!{model: !{item: !chrome.developerPrivate.ExtensionView}}} e
     * @private
     */
    onInspectTap_: function(e) {
      this.delegate.inspectItemView(this.data.id, this.data.views[0]);
    },

    /** @private */
    onExtraInspectTap_: function() {
      this.fire('extension-item-show-details', {data: this.data});
    },

    /** @private */
    onReloadTap_: function() {
      this.delegate.reloadItem(this.data.id);
    },

    /** @private */
    onRepairTap_: function() {
      this.delegate.repairItem(this.data.id);
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
     * Returns true if the enable toggle should be shown.
     * @return {boolean}
     * @private
     */
    showEnableToggle_: function() {
      return !this.isTerminated_() && !this.data.disableReasons.corruptInstall;
    },

    /**
     * Returns true if the extension is in the terminated state.
     * @return {boolean}
     * @private
     */
    isTerminated_: function() {
      return this.data.state ==
          chrome.developerPrivate.ExtensionState.TERMINATED;
    },

    /**
     * return {string}
     * @private
     */
    computeClasses_: function() {
      var classes = this.isEnabled_() ? 'enabled' : 'disabled';
      if (this.inDevMode)
        classes += ' dev-mode';
      return classes;
    },

    /**
     * @return {string}
     * @private
     */
    computeSourceIndicatorIcon_: function() {
      switch (extensions.getItemSource(this.data)) {
        case SourceType.POLICY:
          return 'communication:business';
        case SourceType.SIDELOADED:
          return 'input';
        case SourceType.UNPACKED:
          return 'extensions-icons:unpacked';
        case SourceType.WEBSTORE:
          return '';
      }
      assertNotReached();
    },

    /**
     * @return {string}
     * @private
     */
    computeSourceIndicatorText_: function() {
      var sourceType = extensions.getItemSource(this.data);
      return sourceType == SourceType.WEBSTORE ?
          '' :
          extensions.getItemSourceString(sourceType);
    },

    /**
     * @return {boolean}
     * @private
     */
    computeInspectViewsHidden_: function() {
      return !this.data.views || this.data.views.length == 0;
    },

    /**
     * @return {string}
     * @private
     */
    computeFirstInspectLabel_: function() {
      // Note: theoretically, this wouldn't be called without any inspectable
      // views (because it's in a dom-if="!computeInspectViewsHidden_()").
      // However, due to the recycling behavior of iron list, it seems that
      // sometimes it can. Even when it is, the UI behaves properly, but we
      // need to handle the case gracefully.
      if (this.data.views.length == 0)
        return '';
      var label = extensions.computeInspectableViewLabel(this.data.views[0]);
      if (this.data.views.length > 1)
        label += ',';
      return label;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeExtraViewsHidden_: function() {
      return this.data.views.length <= 1;
    },

    /**
     * @return {boolean}
     * @private
     */
    computeDevReloadButtonHidden_: function() {
      // Only display the reload spinner if the extension is unpacked and
      // not terminated (since if it's terminated, we'll show a crashed reload
      // buton).
      var showIcon =
          this.data.location == chrome.developerPrivate.Location.UNPACKED &&
          this.data.state != chrome.developerPrivate.ExtensionState.TERMINATED;
      return !showIcon;
    },

    /**
     * @return {string}
     * @private
     */
    computeExtraInspectLabel_: function() {
      return loadTimeData.getStringF(
          'itemInspectViewsExtra', this.data.views.length - 1);
    },

    /**
     * @return {boolean}
     * @private
     */
    hasWarnings_: function() {
      return this.data.disableReasons.corruptInstall ||
          this.data.disableReasons.suspiciousInstall ||
          !!this.data.blacklistText;
    },

    /**
     * @return {string}
     * @private
     */
    computeWarningsClasses_: function() {
      return this.data.blacklistText ? 'severe' : 'mild';
    },
  });

  return {
    Item: Item,
    ItemDelegate: ItemDelegate,
  };
});
