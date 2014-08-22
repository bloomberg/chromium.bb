// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Base class for print option section components.
   * @constructor
   * @extends {print_preview.Component}
   */
  function SettingsSection() {
    print_preview.Component.call(this);

    /**
     * Cached "hasCollapsibleContent" status for COLLAPSIBLE_CONTENT_CHANGED
     * notification.
     * @private {?boolean}
     */
    this.hasCollapsibleContentCached_ = null;

    /**
     * Whether content of this section should be collapsed or not.
     * @private {boolean}
     */
    this.collapseContent_ = false;
  };

  /**
   * Event types dispatched by this class.
   * @enum {string}
   */
  SettingsSection.EventType = {
    COLLAPSIBLE_CONTENT_CHANGED:
        'print_preview.SettingsSection.COLLAPSIBLE_CONTENT_CHANGED'
  };

  SettingsSection.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @return {boolean} Whether this section should be displayed or not. */
    isAvailable: function() {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {boolean} Whether this section has a content which can be
     *     collapsed/expanded.
     */
    hasCollapsibleContent: function() {
      throw Error('Abstract method not overridden');
    },

    /** @param {boolean} isEnabled Whether this component is enabled. */
    set isEnabled(isEnabled) {
      throw Error('Abstract method not overridden');
    },

    /**
     * @return {boolean} Whether the content of this section should be
     *     collapsed.
     */
    get collapseContent() {
      return this.collapseContent_;
    },

    /**
     * @param {boolean} collapseContent Whether the content of this section
     *     should be collapsed, even if this section is available.
     */
    set collapseContent(collapseContent) {
      this.collapseContent_ = collapseContent && this.hasCollapsibleContent();
      this.updateUiStateInternal();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      this.isAvailable_ = this.isAvailable();
      if (!this.isAvailable())
        fadeOutOption(this.getElement(), true);
    },

    /**
     * Updates the component appearance according to the current state.
     * @protected
     */
    updateUiStateInternal: function() {
      var hasCollapsibleContent = this.hasCollapsibleContent();
      var changed = this.hasCollapsibleContentCached_ != hasCollapsibleContent;
      this.hasCollapsibleContentCached_ = hasCollapsibleContent;

      if (this.isSectionVisibleInternal())
        fadeInOption(this.getElement());
      else
        fadeOutOption(this.getElement());

      if (changed) {
        cr.dispatchSimpleEvent(
            this, SettingsSection.EventType.COLLAPSIBLE_CONTENT_CHANGED);
      }
    },

    /**
     * @return {boolean} Whether this section should be displayed or not.
     * @protected
     */
    isSectionVisibleInternal: function() {
      return this.isAvailable() && !this.collapseContent_;
    }
  };

  // Export
  return {
    SettingsSection: SettingsSection
  };
});
