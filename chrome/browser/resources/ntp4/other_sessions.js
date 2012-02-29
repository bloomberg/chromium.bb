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

  OtherSessionsMenuButton.prototype = {
    __proto__: MenuButton.prototype,

    decorate: function() {
      MenuButton.prototype.decorate.call(this);
      this.menu = new Menu;
      cr.ui.decorate(this.menu, Menu);
      this.menu.classList.add('footer-menu');
      document.body.appendChild(this.menu);

      this.sessions_ = [];
      this.anchorType = cr.ui.AnchorType.ABOVE;
      this.invertLeftRight = true;

      this.hidden = false;
      this.showPromo_();
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

      MenuButton.prototype.showMenu.call(this);
    },

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
          section.appendChild(a);
        }
      }
    },

    showPromo_: function() {
      var message = localStrings.getString('otherSessionsEmpty');
      this.menu.appendChild(this.ownerDocument.createTextNode(message));
    },

    /**
     * Sets the menu model data.
     * @param {Array} sessionList Array of objects describing the sessions
     * from other devices.
     */
    set sessions(sessionList) {
      // Clear the current contents of the menu.
      this.menu.innerHTML = '';

      // Rebuild the menu with the new data.
      for (var i = 0; i < sessionList.length; i++) {
        this.addSession_(sessionList[i]);
      }

      if (sessionList.length == 0)
        this.showPromo();

      this.sessions_ = sessionList;
    },
  };

  return {
    OtherSessionsMenuButton: OtherSessionsMenuButton,
  };
});
