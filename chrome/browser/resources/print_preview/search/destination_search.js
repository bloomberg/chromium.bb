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
   * @param {!print_preview.InvitationStore} invitationStore Data store
   *     holding printer sharing invitations.
   * @param {!print_preview.UserInfo} userInfo Event target that contains
   *     information about the logged in user.
   * @constructor
   * @extends {print_preview.Overlay}
   */
  function DestinationSearch(destinationStore, invitationStore, userInfo) {
    print_preview.Overlay.call(this);

    /**
     * Data store containing the destinations to search through.
     * @private {!print_preview.DestinationStore}
     */
    this.destinationStore_ = destinationStore;

    /**
     * Data store holding printer sharing invitations.
     * @private {!print_preview.InvitationStore}
     */
    this.invitationStore_ = invitationStore;

    /**
     * Event target that contains information about the logged in user.
     * @private {!print_preview.UserInfo}
     */
    this.userInfo_ = userInfo;

    /**
     * Instance of native layer used to send metrics to C++ metrics handler.
     * @private {!print_preview.NativeLayer}
     */
    this.nativeLayer_ = print_preview.NativeLayer.getInstance();

    /**
     * Currently displayed printer sharing invitation.
     * @private {print_preview.Invitation}
     */
    this.invitation_ = null;

    /**
     * Used to record usage statistics.
     * @private {!print_preview.DestinationSearchMetricsContext}
     */
    this.metrics_ = new print_preview.DestinationSearchMetricsContext();

    /**
     * Whether or not a UMA histogram for the register promo being shown was
     * already recorded.
     * @private {boolean}
     */
    this.registerPromoShownMetricRecorded_ = false;

    /**
     * Child overlay used for resolving a provisional destination. The overlay
     * is shown when the user attempts to select a provisional destination.
     * Set only when a destination is being resolved.
     * @private {?print_preview.ProvisionalDestinationResolver}
     */
    this.provisionalDestinationResolver_ = null;

    /**
     * The destination that is currently in configuration.
     * @private {?print_preview.Destination}
     */
    this.destinationInConfiguring_ = null;

    /**
     * Search box used to search through the destination lists.
     * @private {!print_preview.SearchBox}
     */
    this.searchBox_ = new print_preview.SearchBox(
        loadTimeData.getString('searchBoxPlaceholder'));
    this.addChild(this.searchBox_);

    /**
     * Destination list containing recent destinations.
     * @private {!print_preview.DestinationList}
     */
    this.recentList_ = new print_preview.RecentDestinationList(this);
    this.addChild(this.recentList_);

    /**
     * Destination list containing local destinations.
     * @private {!print_preview.DestinationList}
     */
    this.localList_ = new print_preview.DestinationList(
        this, loadTimeData.getString('localDestinationsTitle'),
        loadTimeData.getBoolean('showLocalManageButton') ?
            loadTimeData.getString('manage') :
            null);
    this.addChild(this.localList_);

    /**
     * Destination list containing cloud destinations.
     * @private {!print_preview.DestinationList}
     */
    this.cloudList_ = new print_preview.CloudDestinationList(this);
    this.addChild(this.cloudList_);
  }

  /**
   * Event types dispatched by the component.
   * @enum {string}
   */
  DestinationSearch.EventType = {
    // Dispatched when user requests to sign-in into another Google account.
    ADD_ACCOUNT: 'print_preview.DestinationSearch.ADD_ACCOUNT',

    // Dispatched when the user requests to manage their cloud destinations.
    MANAGE_CLOUD_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_CLOUD_DESTINATIONS',

    // Dispatched when the user requests to manage their local destinations.
    MANAGE_LOCAL_DESTINATIONS:
        'print_preview.DestinationSearch.MANAGE_LOCAL_DESTINATIONS',

    // Dispatched when the user requests to sign-in to their Google account.
    SIGN_IN: 'print_preview.DestinationSearch.SIGN_IN'
  };

  /**
   * Number of unregistered destinations that may be promoted to the top.
   * @type {number}
   * @const
   * @private
   */
  DestinationSearch.MAX_PROMOTED_UNREGISTERED_PRINTERS_ = 2;

  DestinationSearch.prototype = {
    __proto__: print_preview.Overlay.prototype,

    /** @override */
    onSetVisibleInternal: function(isVisible) {
      if (isVisible) {
        this.searchBox_.focus();
        if (getIsVisible(this.getChildElement('.cloudprint-promo'))) {
          this.metrics_.record(
              print_preview.Metrics.DestinationSearchBucket.SIGNIN_PROMPT);
          this.nativeLayer_.recordAction('Signin_Impression_FromCloudPrint');
        }
        if (this.userInfo_.initialized)
          this.onUsersChanged_();
        this.reflowLists_();
        this.metrics_.record(
            print_preview.Metrics.DestinationSearchBucket.DESTINATION_SHOWN);

        this.destinationStore_.startLoadAllDestinations();
        this.invitationStore_.startLoadingInvitations();
      } else {
        // Collapse all destination lists
        this.localList_.setIsShowAll(false);
        this.cloudList_.setIsShowAll(false);
        if (this.provisionalDestinationResolver_)
          this.provisionalDestinationResolver_.cancel();
        this.resetSearch_();
      }
    },

    /** @override */
    onCancelInternal: function() {
      this.metrics_.record(print_preview.Metrics.DestinationSearchBucket
                               .DESTINATION_CLOSED_UNCHANGED);
    },

    /** Shows the Google Cloud Print promotion banner. */
    showCloudPrintPromo: function() {
      setIsVisible(this.getChildElement('.cloudprint-promo'), true);
      if (this.getIsVisible()) {
        this.metrics_.record(
            print_preview.Metrics.DestinationSearchBucket.SIGNIN_PROMPT);
        this.nativeLayer_.recordAction('Signin_Impression_FromCloudPrint');
      }
      this.reflowLists_();
    },

    /** @override */
    enterDocument: function() {
      print_preview.Overlay.prototype.enterDocument.call(this);

      this.tracker.add(
          this.getChildElement('.account-select'), 'change',
          this.onAccountChange_.bind(this));

      this.tracker.add(
          this.getChildElement('.sign-in'), 'click',
          this.onSignInActivated_.bind(this));

      this.tracker.add(
          this.getChildElement('.invitation-accept-button'), 'click',
          this.onInvitationProcessButtonClick_.bind(this, true /*accept*/));
      this.tracker.add(
          this.getChildElement('.invitation-reject-button'), 'click',
          this.onInvitationProcessButtonClick_.bind(this, false /*accept*/));

      this.tracker.add(
          this.getChildElement('.cloudprint-promo > .close-button'), 'click',
          this.onCloudprintPromoCloseButtonClick_.bind(this));
      this.tracker.add(
          this.searchBox_, print_preview.SearchBox.EventType.SEARCH,
          this.onSearch_.bind(this));
      this.tracker.add(
          this, print_preview.DestinationListItem.EventType.CONFIGURE_REQUEST,
          this.onDestinationConfigureRequest_.bind(this));
      this.tracker.add(
          this, print_preview.DestinationListItem.EventType.SELECT,
          this.onDestinationSelect_.bind(this));
      this.tracker.add(
          this,
          print_preview.DestinationListItem.EventType.REGISTER_PROMO_CLICKED,
          function() {
            this.metrics_.record(print_preview.Metrics.DestinationSearchBucket
                                     .REGISTER_PROMO_SELECTED);
          }.bind(this));

      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATIONS_INSERTED,
          this.onDestinationsInserted_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SELECT,
          this.onDestinationStoreSelect_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SEARCH_STARTED,
          this.updateThrobbers_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType.DESTINATION_SEARCH_DONE,
          this.onDestinationSearchDone_.bind(this));
      this.tracker.add(
          this.destinationStore_,
          print_preview.DestinationStore.EventType
              .PROVISIONAL_DESTINATION_RESOLVED,
          this.onDestinationsInserted_.bind(this));

      this.tracker.add(
          this.invitationStore_,
          print_preview.InvitationStore.EventType.INVITATION_SEARCH_DONE,
          this.updateInvitations_.bind(this));
      this.tracker.add(
          this.invitationStore_,
          print_preview.InvitationStore.EventType.INVITATION_PROCESSED,
          this.updateInvitations_.bind(this));

      this.tracker.add(
          this.localList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageLocalDestinationsActivated_.bind(this));
      this.tracker.add(
          this.cloudList_,
          print_preview.DestinationList.EventType.ACTION_LINK_ACTIVATED,
          this.onManageCloudDestinationsActivated_.bind(this));

      this.tracker.add(
          this.userInfo_, print_preview.UserInfo.EventType.USERS_CHANGED,
          this.onUsersChanged_.bind(this));

      this.tracker.add(
          this.getChildElement('.button-strip .cancel-button'), 'click',
          this.cancel.bind(this));

      this.tracker.add(window, 'resize', this.onWindowResize_.bind(this));

      this.updateThrobbers_();

      // Render any destinations already in the store.
      this.renderDestinations_();
    },

    /** @override */
    decorateInternal: function() {
      this.searchBox_.render(this.getChildElement('.search-box-container'));
      this.recentList_.render(this.getChildElement('.recent-list'));
      this.localList_.render(this.getChildElement('.local-list'));
      this.cloudList_.render(this.getChildElement('.cloud-list'));
      this.getChildElement('.promo-text').innerHTML = loadTimeData.getStringF(
          'cloudPrintPromotion', '<a is="action-link" class="sign-in">',
          '</a>');
      this.getChildElement('.account-select-label').textContent =
          loadTimeData.getString('accountSelectTitle');
    },

    /**
     * @return {number} Height available for destination lists, in pixels.
     * @private
     */
    getAvailableListsHeight_: function() {
      var elStyle = window.getComputedStyle(this.getElement());
      return this.getElement().offsetHeight -
          parseInt(elStyle.getPropertyValue('padding-top'), 10) -
          parseInt(elStyle.getPropertyValue('padding-bottom'), 10) -
          this.getChildElement('.lists').offsetTop -
          this.getChildElement('.invitation-container').offsetHeight -
          this.getChildElement('.cloudprint-promo').offsetHeight -
          this.getChildElement('.action-area').offsetHeight;
    },

    /**
     * Filters all destination lists with the given query.
     * @param {RegExp} query Query to filter destination lists by.
     * @private
     */
    filterLists_: function(query) {
      this.recentList_.updateSearchQuery(query);
      this.localList_.updateSearchQuery(query);
      this.cloudList_.updateSearchQuery(query);
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
     * Renders all of the destinations in the destination store.
     * @private
     */
    renderDestinations_: function() {
      var recentDestinations = [];
      var localDestinations = [];
      var cloudDestinations = [];
      var unregisteredCloudDestinations = [];

      var destinations =
          this.destinationStore_.destinations(this.userInfo_.activeUser);
      destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
        if (destination.isLocal ||
            destination.origin == print_preview.DestinationOrigin.DEVICE) {
          localDestinations.push(destination);
        } else {
          if (destination.connectionStatus ==
              print_preview.DestinationConnectionStatus.UNREGISTERED) {
            unregisteredCloudDestinations.push(destination);
          } else {
            cloudDestinations.push(destination);
          }
        }
      });

      if (unregisteredCloudDestinations.length != 0 &&
          !this.registerPromoShownMetricRecorded_) {
        this.metrics_.record(
            print_preview.Metrics.DestinationSearchBucket.REGISTER_PROMO_SHOWN);
        this.registerPromoShownMetricRecorded_ = true;
      }

      var finalCloudDestinations =
          unregisteredCloudDestinations
              .slice(0, DestinationSearch.MAX_PROMOTED_UNREGISTERED_PRINTERS_)
              .concat(
                  cloudDestinations,
                  unregisteredCloudDestinations.slice(
                      DestinationSearch.MAX_PROMOTED_UNREGISTERED_PRINTERS_));

      this.recentList_.updateDestinations(recentDestinations);
      this.localList_.updateDestinations(localDestinations);
      this.cloudList_.updateDestinations(finalCloudDestinations);
    },

    /**
     * Reflows the destination lists according to the available height.
     * @private
     */
    reflowLists_: function() {
      if (!this.getIsVisible()) {
        return;
      }

      var hasCloudList = getIsVisible(this.getChildElement('.cloud-list'));
      var lists = [this.recentList_, this.localList_];
      if (hasCloudList) {
        lists.push(this.cloudList_);
      }

      var getListsTotalHeight = function(lists, counts) {
        return lists.reduce(function(sum, list, index) {
          var container = list.getContainerElement();
          return sum + list.getEstimatedHeightInPixels(counts[index]) +
              parseInt(window.getComputedStyle(container).paddingBottom, 10);
        }, 0);
      };
      var getCounts = function(lists, count) {
        return lists.map(function(list) {
          return count;
        });
      };

      var availableHeight = this.getAvailableListsHeight_();
      var listsEl = this.getChildElement('.lists');
      listsEl.style.maxHeight = availableHeight + 'px';

      var maxListLength = lists.reduce(function(prevCount, list) {
        return Math.max(prevCount, list.getDestinationsCount());
      }, 0);
      for (var i = 1; i <= maxListLength; i++) {
        if (getListsTotalHeight(lists, getCounts(lists, i)) > availableHeight) {
          i--;
          break;
        }
      }
      var counts = getCounts(lists, i);
      // Fill up the possible n-1 free slots left by the previous loop.
      if (getListsTotalHeight(lists, counts) < availableHeight) {
        for (var countIndex = 0; countIndex < counts.length; countIndex++) {
          counts[countIndex]++;
          if (getListsTotalHeight(lists, counts) > availableHeight) {
            counts[countIndex]--;
            break;
          }
        }
      }

      lists.forEach(function(list, index) {
        list.updateShortListSize(counts[index]);
      });

      // Set height of the list manually so that search filter doesn't change
      // lists height.
      var listsHeight = getListsTotalHeight(lists, counts) + 'px';
      if (listsHeight != listsEl.style.height) {
        // Try to close account select if there's a possibility it's open now.
        var accountSelectEl = this.getChildElement('.account-select');
        if (!accountSelectEl.disabled) {
          accountSelectEl.disabled = true;
          accountSelectEl.disabled = false;
        }
        listsEl.style.height = listsHeight;
      }
    },

    /**
     * Updates whether the throbbers for the various destination lists should be
     * shown or hidden.
     * @private
     */
    updateThrobbers_: function() {
      this.localList_.setIsThrobberVisible(
          this.destinationStore_.isLocalDestinationSearchInProgress);
      this.cloudList_.setIsThrobberVisible(
          this.destinationStore_.isCloudDestinationSearchInProgress);
      this.recentList_.setIsThrobberVisible(
          this.destinationStore_.isLocalDestinationSearchInProgress &&
          this.destinationStore_.isCloudDestinationSearchInProgress);
      this.reflowLists_();
    },

    /**
     * Updates printer sharing invitations UI.
     * @private
     */
    updateInvitations_: function() {
      var invitations = this.userInfo_.activeUser ?
          this.invitationStore_.invitations(this.userInfo_.activeUser) :
          [];
      if (invitations.length > 0) {
        if (this.invitation_ != invitations[0]) {
          this.metrics_.record(print_preview.Metrics.DestinationSearchBucket
                                   .INVITATION_AVAILABLE);
        }
        this.invitation_ = invitations[0];
        this.showInvitation_(this.invitation_);
      } else {
        this.invitation_ = null;
      }
      setIsVisible(
          this.getChildElement('.invitation-container'), !!this.invitation_);
      this.reflowLists_();
    },

    /**
     * @param {!print_preview.Invitation} invitation Invitation to show.
     * @private
     */
    showInvitation_: function(invitation) {
      var invitationText = '';
      if (invitation.asGroupManager) {
        invitationText = loadTimeData.getStringF(
            'groupPrinterSharingInviteText', HTMLEscape(invitation.sender),
            HTMLEscape(invitation.destination.displayName),
            HTMLEscape(invitation.receiver));
      } else {
        invitationText = loadTimeData.getStringF(
            'printerSharingInviteText', HTMLEscape(invitation.sender),
            HTMLEscape(invitation.destination.displayName));
      }
      this.getChildElement('.invitation-text').innerHTML = invitationText;

      var acceptButton = this.getChildElement('.invitation-accept-button');
      acceptButton.textContent = loadTimeData.getString(
          invitation.asGroupManager ? 'acceptForGroup' : 'accept');
      acceptButton.disabled = !!this.invitationStore_.invitationInProgress;
      this.getChildElement('.invitation-reject-button').disabled =
          !!this.invitationStore_.invitationInProgress;
      setIsVisible(
          this.getChildElement('#invitation-process-throbber'),
          !!this.invitationStore_.invitationInProgress);
    },

    /**
     * Called when user's logged in accounts change. Updates the UI.
     * @private
     */
    onUsersChanged_: function() {
      var loggedIn = this.userInfo_.loggedIn;
      if (loggedIn) {
        var accountSelectEl = this.getChildElement('.account-select');
        accountSelectEl.innerHTML = '';
        this.userInfo_.users.forEach(function(account) {
          var option = document.createElement('option');
          option.text = account;
          option.value = account;
          accountSelectEl.add(option);
        });
        var option = document.createElement('option');
        option.text = loadTimeData.getString('addAccountTitle');
        option.value = '';
        accountSelectEl.add(option);

        accountSelectEl.selectedIndex = this.userInfo_.activeUser ?
            this.userInfo_.users.indexOf(this.userInfo_.activeUser) :
            -1;
      }

      setIsVisible(this.getChildElement('.user-info'), loggedIn);
      setIsVisible(this.getChildElement('.cloud-list'), loggedIn);
      setIsVisible(this.getChildElement('.cloudprint-promo'), !loggedIn);
      this.updateInvitations_();
    },

    /**
     * Called when a destination search should be executed. Filters the
     * destination lists with the given query.
     * @param {!Event} event Contains the search query.
     * @private
     */
    onSearch_: function(event) {
      this.filterLists_(event.queryRegExp);
    },

    /**
     * Handler for {@code print_preview.DestinationListItem.EventType.
     * CONFIGURE_REQUEST} event, which is called to check a destination list
     * item needs to be setup on Chrome OS before being selected. Note we do not
     * allow configuring more than one destination at the same time.
     * @param {!CustomEvent} event Contains the destination needs to be setup.
     * @private
     */
    onDestinationConfigureRequest_: function(event) {
      var destination = event.detail.destination;
      // Cloud Print Device printers are stored in the local list
      // crbug.com/713831.
      // TODO(crbug.com/416701): Upon resolution, update this.
      var destinationItem =
          (destination.isLocal ||
           destination.origin == print_preview.DestinationOrigin.DEVICE) ?
          this.localList_.getDestinationItem(destination.id) :
          this.cloudList_.getDestinationItem(destination.id);
      assert(
          destinationItem != null,
          'User does not select a valid destination item.');

      // Another printer setup is in process or the printer doesn't need to be
      // set up. Reject the setup request directly.
      if (this.destinationInConfiguring_ != null ||
          destination.origin != print_preview.DestinationOrigin.CROS ||
          destination.capabilities != null) {
        destinationItem.onConfigureRequestRejected(
            this.destinationInConfiguring_ != null);
      } else {
        destinationItem.onConfigureRequestAccepted();
        this.handleConfigureDestination_(destination);
      }
    },

    /**
     * Called When a destination needs to be setup.
     * @param {!print_preview.Destination} destination The destination needs to
     *     be setup.
     * @private
     */
    handleConfigureDestination_: function(destination) {
      assert(
          destination.origin == print_preview.DestinationOrigin.CROS,
          'Only local printer on Chrome OS requires setup.');
      this.destinationInConfiguring_ = destination;
      this.destinationStore_.resolveCrosDestination(destination)
          .then(
              /**
               * @param {!print_preview.PrinterSetupResponse} response
               */
              function(response) {
                this.destinationInConfiguring_ = null;
                this.localList_.getDestinationItem(destination.id)
                    .onConfigureResolved(response);
              }.bind(this),
              function() {
                this.destinationInConfiguring_ = null;
                this.localList_.getDestinationItem(destination.id)
                    .onConfigureResolved(
                        {printerId: destination.id, success: false});
              }.bind(this));
    },

    /**
     * Handler for {@code print_preview.DestinationListItem.EventType.SELECT}
     * event, which is called when a destination list item is selected.
     * @param {Event} event Contains the selected destination.
     * @private
     */
    onDestinationSelect_: function(event) {
      this.handleOnDestinationSelect_(event.destination);
    },

    /**
     * Called when a destination is selected. Clears the search and hides the
     * widget. If The destination is provisional, it runs provisional
     * destination resolver first.
     * @param {!print_preview.Destination} destination The selected destination.
     * @private
     */
    handleOnDestinationSelect_: function(destination) {
      if (destination.isProvisional) {
        assert(
            !this.provisionalDestinationResolver_,
            'Provisional destination resolver already exists.');
        this.provisionalDestinationResolver_ =
            print_preview.ProvisionalDestinationResolver.create(
                this.destinationStore_, destination);
        assert(
            !!this.provisionalDestinationResolver_,
            'Unable to create provisional destination resolver');

        var lastFocusedElement = document.activeElement;
        this.addChild(this.provisionalDestinationResolver_);
        this.provisionalDestinationResolver_.run(this.getElement())
            .then(
                /**
                 * @param {!print_preview.Destination} resolvedDestination
                 *    Destination to which the provisional destination was
                 *    resolved.
                 */
                function(resolvedDestination) {
                  this.handleOnDestinationSelect_(resolvedDestination);
                }.bind(this))
            .catch(function() {
              console.error(
                  'Failed to resolve provisional destination: ' +
                  destination.id);
            })
            .then(function() {
              this.removeChild(assert(this.provisionalDestinationResolver_));
              this.provisionalDestinationResolver_ = null;

              // Restore focus to the previosly focused element if it's
              // still shown in the search.
              if (lastFocusedElement && this.getIsVisible() &&
                  getIsVisible(lastFocusedElement) &&
                  this.getElement().contains(lastFocusedElement)) {
                lastFocusedElement.focus();
              }
            }.bind(this));
        return;
      }

      this.setIsVisible(false);
      this.destinationStore_.selectDestination(destination);
      this.metrics_.record(print_preview.Metrics.DestinationSearchBucket
                               .DESTINATION_CLOSED_CHANGED);
    },

    /**
     * Called when a destination is selected. Selected destination are marked as
     * recent, so we have to update our recent destinations list.
     * @private
     */
    onDestinationStoreSelect_: function() {
      var destinations =
          this.destinationStore_.destinations(this.userInfo_.activeUser);
      var recentDestinations = [];
      destinations.forEach(function(destination) {
        if (destination.isRecent) {
          recentDestinations.push(destination);
        }
      });
      this.recentList_.updateDestinations(recentDestinations);
      this.reflowLists_();
    },

    /**
     * Called when destinations are inserted into the store. Rerenders
     * destinations.
     * @private
     */
    onDestinationsInserted_: function() {
      this.renderDestinations_();
      this.reflowLists_();
    },

    /**
     * Called when destinations are inserted into the store. Rerenders
     * destinations.
     * @private
     */
    onDestinationSearchDone_: function() {
      this.updateThrobbers_();
      this.renderDestinations_();
      this.reflowLists_();
      // In case user account information was retrieved with this search
      // (knowing current user account is required to fetch invitations).
      this.invitationStore_.startLoadingInvitations();
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
      this.metrics_.record(
          print_preview.Metrics.DestinationSearchBucket.SIGNIN_TRIGGERED);
    },

    /**
     * Called when item in the Accounts list is selected. Initiates active user
     * switch or, for 'Add account...' item, opens Google sign-in page.
     * @private
     */
    onAccountChange_: function() {
      var accountSelectEl = this.getChildElement('.account-select');
      var account =
          accountSelectEl.options[accountSelectEl.selectedIndex].value;
      if (account) {
        this.userInfo_.activeUser = account;
        this.destinationStore_.reloadUserCookieBasedDestinations();
        this.invitationStore_.startLoadingInvitations();
        this.metrics_.record(
            print_preview.Metrics.DestinationSearchBucket.ACCOUNT_CHANGED);
      } else {
        cr.dispatchSimpleEvent(this, DestinationSearch.EventType.ADD_ACCOUNT);
        // Set selection back to the active user.
        for (var i = 0; i < accountSelectEl.options.length; i++) {
          if (accountSelectEl.options[i].value == this.userInfo_.activeUser) {
            accountSelectEl.selectedIndex = i;
            break;
          }
        }
        this.metrics_.record(
            print_preview.Metrics.DestinationSearchBucket.ADD_ACCOUNT_SELECTED);
      }
    },

    /**
     * Called when the printer sharing invitation Accept/Reject button is
     * clicked.
     * @private
     */
    onInvitationProcessButtonClick_: function(accept) {
      this.metrics_.record(
          accept ? print_preview.Metrics.DestinationSearchBucket
                       .INVITATION_ACCEPTED :
                   print_preview.Metrics.DestinationSearchBucket
                       .INVITATION_REJECTED);
      this.invitationStore_.processInvitation(assert(this.invitation_), accept);
      this.updateInvitations_();
    },

    /**
     * Called when the close button on the cloud print promo is clicked. Hides
     * the promo.
     * @private
     */
    onCloudprintPromoCloseButtonClick_: function() {
      setIsVisible(this.getChildElement('.cloudprint-promo'), false);
      this.reflowLists_();
    },

    /**
     * Called when the window is resized. Reflows layout of destination lists.
     * @private
     */
    onWindowResize_: function() {
      this.reflowLists_();
    }
  };

  // Export
  return {DestinationSearch: DestinationSearch};
});
