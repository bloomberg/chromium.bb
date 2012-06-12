// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component used for searching for a print destination.
   * This is a modal dialog that allows the user to search and select a
   * destination to print to. When a destination is selected, it is written to
   * the destination store.
   * @param {!print_preview.DestinationStore} destinationStore Data store
   *     containing the destinations to search through.
   * @param {!print_preview.UserInfo} userInfo Event target that contains
   *     information about the logged in user.
   * @param {!print_preview.Metrics} metrics Used to record usage statistics.
   * @constructor
   * @extends {print_preview.Component}
   */
  function DestinationSearch(destinationStore, userInfo, metrics) {
    print_preview.Component.call(this);

    /**
     * Data store containing the destinations to search through.
     * @type {!print_preview.DestinationStore}
     * @private
     */
    this.destinationStore_ = destinationStore;

    /**
     * Event target that contains information about the logged in user.
     * @type {!print_preview.UserInfo}
     * @private
     */
    this.userInfo_ = userInfo;

    /**
     * Used to record usage statistics.
     * @type {!print_preview.Metrics}
     * @private
     */
    this.metrics_ = metrics;

    /**
     * Search box used to search through the destination lists.
     * @type {!print_preview.SearchBox}
     * @private
     */
    this.searchBox_ = new print_preview.SearchBox();
    this.addChild(this.searchBox_);

    /**
     * Destination list containing recent destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.recentList_ = new print_preview.DestinationList(
        this,
        localStrings.getString('recentDestinationsTitle'),
        3 /*opt_maxSize*/);
    this.addChild(this.recentList_);

    /**
     * Destination list containing local destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.localList_ = new print_preview.DestinationList(
        this,
        localStrings.getString('localDestinationsTitle'),
        0 /*opt_maxSize*/,
        cr.isChromeOS ? null : localStrings.getString('manage'));
    this.addChild(this.localList_);

    /**
     * Destination list containing cloud destinations.
     * @type {!print_preview.DestinationList}
     * @private
     */
    this.cloudList_ = new print_preview.CloudDestinationList(this);
    this.addChild(this.cloudList_);

    /**
     * Page element of the overlay.
     * @type {HTMLElement}
     * @private
     */
    this.pageEl_ = null;
  };

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSearch.EventType = {
    // Dispatched when the user requests to manage their cloud destinations.
    MANAGE_CLOUD_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_CLOUD_DESTINATIONS',

    // Dispatched when the user requests to manage their local destinations.
    MANAGE_LOCAL_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_LOCAL_DESTINATIONS',

    // Dispatched when the user requests to sign-in to their Google account.
    SIGN_IN: 'print_preview.DestinationSearch.SIGN_IN'
  },

  /**
   * CSS classes used by the component.
   * @enum {string}
   * @private
   */
  DestinationSearch.Classes_ = {
    CLOSE_BUTTON: 'destination-search-close-button',
    CLOUDPRINT_PROMO: 'destination-search-cloudprint-promo',
    CLOUDPRINT_PROMO_CLOSE_BUTTON:
        'destination-search-cloudprint-promo-close-button',
    CLOUD_LIST: 'destination-search-cloud-list',
    LOCAL_LIST: 'destination-search-local-list',
    PAGE: 'destination-search-page',
    PROMO_TEXT: 'destination-search-cloudprint-promo-text',
    PULSE: 'pulse',
    RECENT_LIST: 'destination-search-recent-list',
    SEARCH_BOX_CONTAINER: 'destination-search-search-box-container',
    SIGN_IN: 'destination-search-sign-in',
    TRANSPARENT: 'transparent',
    USER_EMAIL: 'destination-search-user-email',
    USER_INFO: 'destination-search-user-info'
  };

  DestinationSearch.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @return {boolean} Whether the component is visible. */
    getIsVisible: function() {
      return !this.getElement().classList.contains(
          DestinationSearch.Classes_.TRANSPARENT);
    },

    /** @param {boolean} isVisible Whether the component is visible. */
    setIsVisible: function(isVisible) {
      if (isVisible) {
        this.searchBox_.focus();
        this.getElement().classList.remove(
            DestinationSearch.Classes_.TRANSPARENT);
        var promoEl = this.getElement().getElementsByClassName(
            DestinationSearch.Classes_.CLOUDPRINT_PROMO)[0];
        if (getIsVisible(promoEl)) {
          this.metrics_.increment(
              print_preview.Metrics.Bucket.CLOUDPRINT_PROMO_SHOWN);
        }
      } else {
        this.getElement().classList.add(DestinationSearch.Classes_.TRANSPARENT);
        // Collapse all destination lists
        this.localList_.setIsShowAll(false);
        this.cloudList_.setIsShowAll(false);
        this.searchBox_.setQuery('');
        this.filterLists_(null);
      }
    },

    /** @param {string} email Email of the logged-in user. */
    setCloudPrintEmail: function(email) {
      var userEmailEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.USER_EMAIL)[0];
      userEmailEl.textContent = email;
      var userInfoEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.USER_INFO)[0];
      setIsVisible(userInfoEl, true);
      var cloudListEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.CLOUD_LIST)[0];
      setIsVisible(cloudListEl, true);
      var promoEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.CLOUDPRINT_PROMO)[0];
      setIsVisible(promoEl, false);
    },

    /** Shows the Google Cloud Print promotion banner. */
    showCloudPrintPromo: function() {
      var promoEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.CLOUDPRINT_PROMO)[0];
      setIsVisible(promoEl, true);
      if (this.getIsVisible()) {
        this.metrics_.increment(
            print_preview.Metrics.Bucket.CLOUDPRINT_PROMO_SHOWN);
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      var closeButtonEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.CLOSE_BUTTON)[0];
      var signInButton = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.SIGN_IN)[0];
      var cloudprintPromoCloseButton = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.CLOUDPRINT_PROMO_CLOSE_BUTTON)[0];

      this.tracker.add(
          closeButtonEl, 'click', this.onCloseClick_.bind(this));

      var promoTextEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.PROMO_TEXT)[0];
      var signInEl = promoTextEl.getElementsByClassName(
          DestinationSearch.Classes_.SIGN_IN)[0];
      this.tracker.add(
          signInEl, 'click', this.onSignInActivated_.bind(this));

      this.tracker.add(
          cloudprintPromoCloseButton,
          'click',
          this.onCloudprintPromoCloseButtonClick_.bind(this));
      this.tracker.add(
          this.searchBox_,
          print_preview.SearchBox.EventType.SEARCH,
          this.onSearch_.bind(this));
      this.tracker.add(
          this,
          print_preview.DestinationListItem.EventType.SELECT,
          this.onDestinationSelect_.bind(this));

      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
          this.onDestinationsInserted_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationStoreSelect_.bind(this));

      this.tracker.add(
          this.localList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageLocalDestinationsActivated_.bind(this));
      this.tracker.add(
          this.cloudList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageCloudDestinationsActivated_.bind(this));

      this.tracker.add(
          this.getElement(), 'click', this.onClick_.bind(this));
      this.tracker.add(
          this.pageEl_, 'webkitAnimationEnd', this.onAnimationEnd_.bind(this));

      this.tracker.add(
          this.userInfo_,
          print_preview.UserInfo.EventType.EMAIL_CHANGE,
          this.onEmailChange_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.pageEl_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.searchBox_.decorate($('search-box'));

      var recentListEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.RECENT_LIST)[0];
      this.recentList_.render(recentListEl);

      var localListEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.LOCAL_LIST)[0];
      this.localList_.render(localListEl);

      var cloudListEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.CLOUD_LIST)[0];
      this.cloudList_.render(cloudListEl);

      this.pageEl_ = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.PAGE)[0];

      var promoTextEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.PROMO_TEXT)[0];
      promoTextEl.innerHTML = localStrings.getStringF(
          'cloudPrintPromotion',
          '<span class="destination-search-sign-in link-button">',
          '</span>');
    },

    /**
     * Filters all destination lists with the given query.
     * @param {?string} query Query to filter destination lists by.
     * @private
     */
    filterLists_: function(query) {
      this.recentList_.filter(query);
      this.localList_.filter(query);
      this.cloudList_.filter(query);
    },

    /**
     * Resets the search query.
     * @private
     */
    resetSearch_: function() {
      this.searchBox_.setQuery(null);
      this.filterLists_(null);
    },

    /**
     * Called when a destination search should be executed. Filters the
     * destination lists with the given query.
     * @param {cr.Event} evt Contains the search query.
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
      this.setIsVisible(false);
      this.resetSearch_();
      this.metrics_.increment(
          print_preview.Metrics.Bucket.DESTINATION_SELECTION_CANCELED);
    },

    /**
     * Called when a destination is selected. Clears the search and hides the
     * widget.
     * @param {cr.Event} evt Contains the selected destination.
     * @private
     */
    onDestinationSelect_: function(evt) {
      this.setIsVisible(false);
      this.resetSearch_();
      this.destinationStore_.selectDestination(evt.destination);
      this.metrics_.increment(
          print_preview.Metrics.Bucket.DESTINATION_SELECTED);
    },

    /**
     * Called when destinations are added to the destination store. Refreshes UI
     * with new destinations.
     * @private
     */
    onDestinationsInserted_: function() {
      var recentDestinations = [];
      var localDestinations = [];
      var cloudDestinations = [];
      this.destinationStore_.destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
        if (destination.isLocal) {
          localDestinations.push(destination);
        } else {
          cloudDestinations.push(destination);
        }
      });
      this.recentList_.updateDestinations(recentDestinations);
      this.localList_.updateDestinations(localDestinations);
      this.cloudList_.updateDestinations(cloudDestinations);
    },

    /**
     * Called when a destination is selected. Selected destination are marked as
     * recent, so we have to update our recent destinations list.
     * @private
     */
    onDestinationStoreSelect_: function() {
      var destinations = this.destinationStore_.destinations;
      var recentDestinations = [];
      destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
      });
      this.recentList_.updateDestinations(recentDestinations);
    },

    /**
     * Called when the manage cloud printers action is activated.
     * @private
     */
    onManageCloudDestinationsActivated_: function() {
      cr.dispatchSimpleEvent(
          this,
          print_preview.DestinationSearch.EventType.MANAGE_CLOUD_DESTINATIONS);
    },

    /**
     * Called when the manage local printers action is activated.
     * @private
     */
    onManageLocalDestinationsActivated_: function() {
      cr.dispatchSimpleEvent(
          this,
          print_preview.DestinationSearch.EventType.MANAGE_LOCAL_DESTINATIONS);
    },

    /**
     * Called when the "Sign in" link on the Google Cloud Print promo is
     * activated.
     * @private
     */
    onSignInActivated_: function() {
      cr.dispatchSimpleEvent(this, DestinationSearch.EventType.SIGN_IN);
      this.metrics_.increment(print_preview.Metrics.Bucket.SIGNIN_TRIGGERED);
    },

    /**
     * Called when the close button on the cloud print promo is clicked. Hides
     * the promo.
     * @private
     */
    onCloudprintPromoCloseButtonClick_: function() {
      var promoEl = this.getElement().getElementsByClassName(
          DestinationSearch.Classes_.CLOUDPRINT_PROMO)[0];
      setIsVisible(promoEl, false);
    },

    /**
     * Called when the overlay is clicked. Pulses the page.
     * @param {Event} event Contains the element that was clicked.
     * @private
     */
    onClick_: function(event) {
      if (event.target == this.getElement()) {
        this.pageEl_.classList.add(DestinationSearch.Classes_.PULSE);
      }
    },

    /**
     * Called when an animation ends on the page.
     * @private
     */
    onAnimationEnd_: function() {
      this.pageEl_.classList.remove(DestinationSearch.Classes_.PULSE);
    },

    /**
     * Called when the user's email field has changed. Updates the UI.
     * @private
     */
    onEmailChange_: function() {
      var userEmail = this.userInfo_.getUserEmail();
      if (userEmail) {
        this.setCloudPrintEmail(userEmail);
      }
    }
  };

  // Export
  return {
    DestinationSearch: DestinationSearch
  };
});
