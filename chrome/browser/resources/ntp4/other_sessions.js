// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The menu that shows tabs from sessions on other devices.
 */

cr.define('ntp', function() {
  'use strict';

  var localStrings = new LocalStrings();
  var Menu = cr.ui.Menu;
  var MenuItem = cr.ui.MenuItem;
  var MenuButton = cr.ui.MenuButton;
  var OtherSessionsMenuButton = cr.ui.define('button');

  // Histogram buckets for UMA tracking of menu usage.
  var HISTOGRAM_EVENT = {
      INITIALIZED: 0,
      SHOW_MENU: 1,
      LINK_CLICKED: 2,
      LINK_RIGHT_CLICKED: 3
  };
  var HISTOGRAM_EVENT_LIMIT = HISTOGRAM_EVENT.LINK_RIGHT_CLICKED + 1;

  OtherSessionsMenuButton.prototype = {
    __proto__: MenuButton.prototype,

    decorate: function() {
      MenuButton.prototype.decorate.call(this);
      this.menu = new Menu;
      cr.ui.decorate(this.menu, Menu);
      this.menu.classList.add('footer-menu');
      this.menu.addEventListener('contextmenu',
                                 this.onContextMenu_.bind(this), true);
      document.body.appendChild(this.menu);

      this.promoMessage_ = $('other-sessions-promo-template').cloneNode(true);
      this.promoMessage_.removeAttribute('id');  // Prevent a duplicate id.

      this.sessions_ = [];
      this.anchorType = cr.ui.AnchorType.ABOVE;
      this.invertLeftRight = true;

      this.recordUmaEvent_(HISTOGRAM_EVENT.INITIALIZED);
    },

    /**
     * Initialize this element.
     * @param {boolean} signedIn Is the current user signed in?
     */
    initialize: function(signedIn) {
      this.updateSignInState(signedIn);
    },

    /**
     * Record an event in the UMA histogram.
     * @param {Number} eventId The id of the event to be recorded.
     */
    recordUmaEvent_: function(eventId) {
      chrome.send('metricsHandler:recordInHistogram',
          ['NewTabPage.OtherSessionsMenu', eventId, HISTOGRAM_EVENT_LIMIT]);
    },

    /**
     * Handle a context menu event for an object in the menu's DOM subtree.
     */
    onContextMenu_: function(e) {
      // Only record the action if it occurred in one of the menu items.
      if (findAncestorByClass(e.target, 'footer-menu-item'))
        this.recordUmaEvent_(HISTOGRAM_EVENT.LINK_RIGHT_CLICKED);
    },

    /**
     * Shows the menu, first rebuilding it if necessary.
     * TODO(estade): the right of the menu should align with the right of the
     * button.
     * @override
     */
    showMenu: function() {
      if (this.sessions_.length == 0)
        chrome.send('getForeignSessions');
      this.recordUmaEvent_(HISTOGRAM_EVENT.SHOW_MENU);
      MenuButton.prototype.showMenu.call(this);
    },

    /**
     * Reset the menu contents to the default state.
     * @private
     */
    resetMenuContents_: function() {
      this.menu.innerHTML = '';
      this.menu.appendChild(this.promoMessage_);
    },

    /**
     * Create a custom click handler for a link, so that clicking on a link
     * restores the session (including back stack) rather than just opening
     * the URL.
     */
    makeClickHandler_: function(sessionTag, windowId, tabId) {
      var self = this;
      return function(e) {
        self.recordUmaEvent_(HISTOGRAM_EVENT.LINK_CLICKED);
        chrome.send('openForeignSession', [sessionTag, windowId, tabId,
            e.button, e.altKey, e.ctrlKey, e.metaKey, e.shiftKey]);
        e.preventDefault();
      };
    },

    /**
     * Add the UI for a foreign session to the menu.
     * @param {Object} session Object describing the foreign session.
     */
    addSession_: function(session) {
      var doc = this.ownerDocument;

      var section = doc.createElement('section');
      this.menu.appendChild(section);

      var heading = doc.createElement('h3');
      heading.textContent = session.name;
      section.appendChild(heading);

      for (var i = 0; i < session.windows.length; i++) {
        var window = session.windows[i];
        for (var j = 0; j < window.tabs.length; j++) {
          var tab = window.tabs[j];
          var a = doc.createElement('a');
          a.className = 'footer-menu-item';
          a.textContent = tab.title;
          a.href = tab.url;
          a.style.backgroundImage = 'url(chrome://favicon/' + tab.url + ')';
          var clickHandler = this.makeClickHandler_(
              session.tag, String(window.sessionId), String(tab.sessionId));
          a.addEventListener('click', clickHandler);
          section.appendChild(a);
        }
      }
    },

    /**
     * Sets the menu model data. An empty list means that either there are no
     * foreign sessions, or tab sync is disabled for this profile.
     * |isTabSyncEnabled| makes it possible to distinguish between the cases.
     *
     * @param {Array} sessionList Array of objects describing the sessions
     *     from other devices.
     * @param {boolean} isTabSyncEnabled Is tab sync enabled for this profile?
     */
    setForeignSessions: function(sessionList, isTabSyncEnabled) {
      this.sessions_ = sessionList;
      this.resetMenuContents_();
      if (sessionList.length > 0) {
        // Rebuild the menu with the new data.
        for (var i = 0; i < sessionList.length; i++) {
          this.addSession_(sessionList[i]);
        }
      }

      // The menu button is shown iff tab sync is enabled.
      if (isTabSyncEnabled)
        this.classList.remove('invisible');
      else
        this.classList.add('invisible');
    },

    /**
     * Called when this element is initialized, and from the new tab page when
     * the user's signed in state changes,
     * @param {boolean} signedIn Is the user currently signed in?
     */
    updateSignInState: function(signedIn) {
      if (signedIn)
        chrome.send('getForeignSessions');
      else
        this.classList.add('invisible');
    },
  };

  return {
    OtherSessionsMenuButton: OtherSessionsMenuButton,
  };
});
