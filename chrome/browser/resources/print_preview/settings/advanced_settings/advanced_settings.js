// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Modal dialog for print destination's advanced settings.
   * @param {!print_preview.Metrics} metrics Used to record usage statistics.
   * @constructor
   * @extends {print_preview.Component}
   */
  function AdvancedSettings(metrics) {
    print_preview.Component.call(this);

    /** @private {!print_preview.Metrics} */
    this.metrics_ = metrics;

    /** @private {!print_preview.SearchBox} */
    this.searchBox_ = new print_preview.SearchBox(
        localStrings.getString('advancedSettingsSearchBoxPlaceholder'));
    this.addChild(this.searchBox_);

    /** @private {print_preview.Destination} */
    this.destination_ = null;
  };

  AdvancedSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /**
     * @param {!print_preview.Destination} destination Destination to show
     *     advanced settings for.
     */
    showForDestination: function(destination) {
      //assert(!this.destination_);
      this.destination_ = destination;
      this.getChildElement('.advanced-settings-title').textContent =
          localStrings.getStringF('advancedSettingsDialogTitle',
                                  this.destination_.displayName);
      this.setIsVisible_(true);
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);

      this.getElement().addEventListener('webkitTransitionEnd', function f(e) {
        if (e.target != e.currentTarget || e.propertyName != 'opacity')
          return;
        if (e.target.classList.contains('transparent'))
          setIsVisible(e.target, false);
      });

      this.tracker.add(
          this.getChildElement('.page > .close-button'),
          'click',
          this.onCloseClick_.bind(this));
      this.tracker.add(
          this.getChildElement('#cancel-button'),
          'click',
          this.onCloseClick_.bind(this));

      this.tracker.add(
          this.searchBox_,
          print_preview.SearchBox.EventType.SEARCH,
          this.onSearch_.bind(this));

      this.tracker.add(
          this.getElement(), 'click', this.onOverlayClick_.bind(this));
      this.tracker.add(
          this.getChildElement('.page'),
          'webkitAnimationEnd',
          this.onAnimationEnd_.bind(this));

      this.renderSettings_();
    },

    /** @override */
    decorateInternal: function() {
      this.searchBox_.render(this.getChildElement('.search-box-container'));
    },

    /**
     * @return {boolean} Whether the component is visible.
     * @private
     */
    getIsVisible_: function() {
      return !this.getElement().classList.contains('transparent');
    },

    /**
     * @param {boolean} isVisible Whether the component is visible.
     * @private
     */
    setIsVisible_: function(isVisible) {
      if (this.getIsVisible_() == isVisible)
        return;
      if (isVisible) {
        setIsVisible(this.getElement(), true);
        setTimeout(function(element) {
          element.classList.remove('transparent');
        }.bind(this, this.getElement()), 0);
        this.searchBox_.focus();
      } else {
        this.getElement().classList.add('transparent');
        this.searchBox_.setQuery('');
        this.filterLists_(null);
        this.destination_ = null;
      }
    },

    /**
     * @return {number} Height available for settings lists, in pixels.
     * @private
     */
    getAvailableListsHeight_: function() {
      var elStyle = window.getComputedStyle(this.getElement());
      return this.getElement().offsetHeight -
          parseInt(elStyle.getPropertyValue('padding-top')) -
          parseInt(elStyle.getPropertyValue('padding-bottom')) -
          this.getChildElement('.lists').offsetTop;
    },

    /**
     * Filters displayed settings with the given query.
     * @param {?string} query Query to filter settings by.
     * @private
     */
    filterLists_: function(query) {
    },

    /**
     * Resets the filter query.
     * @private
     */
    resetSearch_: function() {
      this.searchBox_.setQuery(null);
      this.filterLists_(null);
    },

    /**
     * Renders all of the available settings.
     * @private
     */
    renderSettings_: function() {
    },

    /**
     * Called when settings search query changes. Filters displayed settings
     * with the given query.
     * @param {Event} evt Contains search query.
     * @private
     */
    onSearch_: function(evt) {
      this.filterLists_(evt.query);
    },

    /**
     * Called when the close button is clicked. Hides the search widget.
     * @private
     */
    onCloseClick_: function() {
      this.setIsVisible_(false);
      this.resetSearch_();
    },

    /**
     * Called when the overlay is clicked. Pulses the page.
     * @param {Event} event Contains the element that was clicked.
     * @private
     */
    onOverlayClick_: function(event) {
      if (event.target == this.getElement())
        this.getChildElement('.page').classList.add('pulse');
    },

    /**
     * Called when an animation ends on the page.
     * @private
     */
    onAnimationEnd_: function() {
      this.getChildElement('.page').classList.remove('pulse');
    }
  };

  // Export
  return {
    AdvancedSettings: AdvancedSettings
  };
});
