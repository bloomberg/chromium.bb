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
     * @param {string} id,
     * @param {chrome.developerPrivate.ExtensionView} view
     */
    inspectItemView: assertNotReached,
  };

  var Item = Polymer({
    is: 'extensions-item',

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

      // Whether or not the expanded view of the item is shown.
      showingDetails_: {
        type: Boolean,
        value: false,
      },

      // The underlying ExtensionInfo itself. Public for use in declarative
      // bindings.
      /** @type {chrome.developerPrivate.ExtensionInfo} */
      data: {
        type: Object,
      },
    },

    behaviors: [
      I18nBehavior,
    ],

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

    /** @private */
    onShowDetailsTap_: function() {
      this.showingDetails_ = !this.showingDetails_;
    },

    /** @private */
    onDeleteTap_: function() {
      this.delegate.deleteItem(this.data.id);
    },

    /** @private */
    onEnableChange_: function() {
      this.delegate.setItemEnabled(this.data.id, this.$.enabled.checked);
    },

    /** @private */
    onDetailsTap_: function() {
      this.fire('extension-item-show-details', {element: this});
    },

    /** @private */
    onAllowIncognitoChange_: function() {
      this.delegate.setItemAllowedIncognito(
          this.data.id, this.$$('#allow-incognito').checked);
    },

    /**
     * @param {!{model: !{item: !chrome.developerPrivate.ExtensionView}}} e
     * @private
     */
    onInspectTap_: function(e) {
      this.delegate.inspectItemView(this.data.id, e.model.item);
    },

    /**
     * Returns true if the extension is enabled, including terminated
     * extensions.
     * @return {boolean}
     * @private
     */
    isEnabled_: function() {
      switch (this.data.state) {
        case chrome.developerPrivate.ExtensionState.ENABLED:
        case chrome.developerPrivate.ExtensionState.TERMINATED:
          return true;
        case chrome.developerPrivate.ExtensionState.DISABLED:
          return false;
      }
      assertNotReached();  // FileNotFound.
    },

    /** @private */
    computeClasses_: function() {
      return this.isEnabled_() ? 'enabled' : 'disabled';
    },

    /** @private */
    computeExpandIcon_: function() {
      return this.showingDetails_ ? 'expand-less' : 'expand-more';
    },

    /** @private */
    computeEnableCheckboxLabel_: function() {
      return this.i18n(this.isEnabled_() ? 'itemEnabled' : 'itemDisabled');
    },

    /**
     * @param {chrome.developerPrivate.ExtensionView} view
     * @private
     */
    computeInspectLabel_: function(view) {
      // Trim the "chrome-extension://<id>/".
      var url = new URL(view.url);
      var label = view.url;
      if (url.protocol == 'chrome-extension:')
        label = url.pathname.substring(1);
      if (label == '_generated_background_page.html')
        label = this.i18n('viewBackgroundPage');
      // Add any qualifiers.
      label += (view.incognito ? ' ' + this.i18n('viewIncognito') : '') +
               (view.renderProcessId == -1 ?
                    ' ' + this.i18n('viewInactive') : '') +
               (view.isIframe ? ' ' + this.i18n('viewIframe') : '');
      return label;
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

