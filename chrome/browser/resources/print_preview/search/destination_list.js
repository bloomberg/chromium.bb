// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that displays a list of destinations with a heading, action link,
   * and "Show All..." button. An event is dispatched when the action link is
   * activated.
   * @param {!cr.EventTarget} eventTarget Event target to pass to destination
   *     items for dispatching SELECT events.
   * @param {string} title Title of the destination list.
   * @param {number=} opt_maxSize Maximum size of the list. If not specified,
   *     defaults to no max.
   * @param {string=} opt_actionLinkLabel Optional label of the action link. If
   *     no label is provided, the action link will not be shown.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationList(
      eventTarget, title, opt_maxSize, opt_actionLinkLabel) {
    print_preview.Component.call(this);

    /**
     * Event target to pass to destination items for dispatching SELECT events.
     * @type {!cr.EventTarget}
     * @private
     */
    this.eventTarget_ = eventTarget;

    /**
     * Title of the destination list.
     * @type {string}
     * @private
     */
    this.title_ = title;

    /**
     * Maximum size of the destination list.
     * @type {number}
     * @private
     */
    this.maxSize_ = opt_maxSize || 0;
    assert(this.maxSize_ <= DestinationList.SHORT_LIST_SIZE_,
           'Max size must be less than or equal to ' +
               DestinationList.SHORT_LIST_SIZE_);

    /**
     * Label of the action link.
     * @type {?string}
     * @private
     */
    this.actionLinkLabel_ = opt_actionLinkLabel || null;

    /**
     * Backing store for the destination list.
     * @type {!Array.<print_preview.Destination>}
     * @private
     */
    this.destinations_ = [];

    /**
     * Current query used for filtering.
     * @type {?string}
     * @private
     */
    this.query_ = null;

    /**
     * Whether the destination list is fully expanded.
     * @type {boolean}
     * @private
     */
    this.isShowAll_ = false;

    /**
     * Message to show when no destinations are available.
     * @type {HTMLElement}
     * @private
     */
    this.noDestinationsMessageEl_ = null;

    /**
     * Footer of the list.
     * @type {HTMLElement}
     * @private
     */
    this.footerEl_ = null;

    /**
     * Container for the destination list items.
     * @type {HTMLElement}
     * @private
     */
    this.destinationListItemContainer_ = null;
  };

  /**
   * Enumeration of event types dispatched by the destination list.
   * @enum {string}
   */
  DestinationList.EventType = {
    // Dispatched when the action linked is activated.
    ACTION_LINK_ACTIVATED: 'print_preview.DestinationList.ACTION_LINK_ACTIVATED'
  };

  /**
   * Classes used by the destination list.
   * @enum {string}
   * @private
   */
  DestinationList.Classes_ = {
    ACTION_LINK: 'destination-list-action-link',
    FOOTER: 'destination-list-footer',
    NO_PRINTERS_MESSAGE: 'destination-list-no-destinations-message',
    PRINTER_ITEM_CONTAINER: 'destination-list-destination-list-item-container',
    SHOW_ALL_BUTTON: 'destination-list-show-all-button',
    TITLE: 'destination-list-title',
    TOTAL: 'destination-list-total'
  };

  /**
   * Maximum number of destinations before showing the "Show All..." button.
   * @type {number}
   * @const
   * @private
   */
  DestinationList.SHORT_LIST_SIZE_ = 4;

  DestinationList.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isShowAll Whether the show-all button is activated. */
    setIsShowAll: function(isShowAll) {
      this.isShowAll_ = isShowAll;
      this.renderDestinations_();
    },

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal(
          'destination-list-template'));

      var titleEl = this.getElement().getElementsByClassName(
          DestinationList.Classes_.TITLE)[0];
      titleEl.textContent = this.title_;

      var actionLinkEl = this.getElement().getElementsByClassName(
            DestinationList.Classes_.ACTION_LINK)[0];
      if (this.actionLinkLabel_) {
        actionLinkEl.textContent = this.actionLinkLabel_;
      } else {
        setIsVisible(actionLinkEl, false);
      }

      this.noDestinationsMessageEl_ = this.getElement().getElementsByClassName(
          DestinationList.Classes_.NO_PRINTERS_MESSAGE)[0];
      this.footerEl_ = this.getElement().getElementsByClassName(
          DestinationList.Classes_.FOOTER)[0];
      this.destinationListItemContainer_ =
          this.getElement().getElementsByClassName(
              DestinationList.Classes_.PRINTER_ITEM_CONTAINER)[0];
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      var actionLinkEl = this.getElement().getElementsByClassName(
          DestinationList.Classes_.ACTION_LINK)[0];
      var showAllButton = this.getElement().getElementsByClassName(
          DestinationList.Classes_.SHOW_ALL_BUTTON)[0];
      this.tracker.add(
          actionLinkEl, 'click', this.onActionLinkClick_.bind(this));
      this.tracker.add(
          showAllButton, 'click', this.setIsShowAll.bind(this, true));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.noDestinationsMessageEl_ = null;
      this.footerEl_ = null;
      this.destinationListItemContainer_ = null;
    },

    /**
     * Updates the destinations to render in the destination list.
     * @param {!Array.<print_preview.Destination>} destinations Destinations to
     *     render.
     */
    updateDestinations: function(destinations) {
      this.destinations_ = destinations;
      this.renderDestinations_();
    },

    /**
     * Filters the destination list with the given query.
     * @param {?string} query Query to filter the list with.
     */
    filter: function(query) {
      this.query_ = query;
      this.renderDestinations_();
    },

    /**
     * @param {string} text Text to set the action link to.
     * @protected
     */
    setActionLinkTextInternal: function(text) {
      this.actionLinkLabel_ = text;
      var actionLinkEl = this.getElement().getElementsByClassName(
            DestinationList.Classes_.ACTION_LINK)[0];
      actionLinkEl.textContent = this.actionLinkLabel_;
    },

    /**
     * Renders all destinations in the list that match the current query. For
     * each render, all old destination items are first removed.
     * @private
     */
    renderDestinations_: function() {
      this.removeChildren();

      var filteredDests = [];
      this.destinations_.forEach(function(destination) {
        if (!this.query_ || destination.matches(this.query_)) {
          filteredDests.push(destination);
        }
      }, this);

      // TODO(rltoscano): Sort filtered list?

      if (filteredDests.length == 0) {
        this.renderEmptyList_();
      } else if (this.maxSize_) {
        this.renderListWithMaxSize_(filteredDests);
      } else {
        this.renderListWithNoMaxSize_(filteredDests);
      }
    },

    /**
     * Renders a "No destinations found" element.
     * @private
     */
    renderEmptyList_: function() {
      setIsVisible(this.noDestinationsMessageEl_, true);
      setIsVisible(this.footerEl_, false);
    },

    /**
     * Renders the list of destinations up to the maximum size.
     * @param {!Array.<print_preview.Destination>} filteredDests Filtered list
     *     of print destinations to render.
     * @private
     */
    renderListWithMaxSize_: function(filteredDests) {
      setIsVisible(this.noDestinationsMessageEl_, false);
      setIsVisible(this.footerEl_, false);
      for (var dest, i = 0;
           i < this.maxSize_ && (dest = filteredDests[i]);
           i++) {
        var destListItem = new print_preview.DestinationListItem(
            this.eventTarget_, dest);
        this.addChild(destListItem);
        destListItem.render(this.destinationListItemContainer_);
      }
    },

    /**
     * Renders all destinations in the given list.
     * @param {!Array.<print_preview.Destination>} filteredDests Filtered list
     *     of print destinations to render.
     * @private
     */
    renderListWithNoMaxSize_: function(filteredDests) {
      setIsVisible(this.noDestinationsMessageEl_, false);
      if (filteredDests.length <= DestinationList.SHORT_LIST_SIZE_ ||
          this.isShowAll_) {
        filteredDests.forEach(function(dest) {
          var destListItem = new print_preview.DestinationListItem(
              this.eventTarget_, dest);
          this.addChild(destListItem);
          destListItem.render(this.destinationListItemContainer_);
        }, this);
        setIsVisible(this.footerEl_, false);
      } else {
        for (var dest, i = 0; i < DestinationList.SHORT_LIST_SIZE_ - 1; i++) {
          var destListItem = new print_preview.DestinationListItem(
              this.eventTarget_, filteredDests[i]);
          this.addChild(destListItem);
          destListItem.render(this.destinationListItemContainer_);
        }
        setIsVisible(this.footerEl_, true);
        var totalCountEl = this.getElement().getElementsByClassName(
            DestinationList.Classes_.TOTAL)[0];
        totalCountEl.textContent =
            localStrings.getStringF('destinationCount', filteredDests.length);
      }
    },

    /**
     * Called when the action link is clicked. Dispatches an
     * ACTION_LINK_ACTIVATED event.
     * @private
     */
    onActionLinkClick_: function() {
      cr.dispatchSimpleEvent(
          this, DestinationList.EventType.ACTION_LINK_ACTIVATED);
    }
  };

  // Export
  return {
    DestinationList: DestinationList
  };
});
