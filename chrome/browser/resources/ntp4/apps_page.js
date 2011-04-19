// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var TilePage = ntp4.TilePage;

  /**
   * Creates a new App object.
   * @param {Object} appData The data object that describes the app.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function App(appData) {
    var el = cr.doc.createElement('div');
    el.__proto__ = App.prototype;
    el.appData = appData;
    el.initialize();

    return el;
  }

  App.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      assert(this.appData.id, 'Got an app without an ID');

      this.className = 'app';
      this.setAttribute('app-id', this.appData.id);

      var appImg = this.ownerDocument.createElement('img');
      appImg.src = this.appData.icon_big;
      // We use a mask of the same image so CSS rules can highlight just the
      // image when it's touched.
      appImg.style.WebkitMaskImage = url(this.appData.icon_big);
      // We put a click handler just on the app image - so clicking on the
      // margins between apps doesn't do anything.
      appImg.addEventListener('click', this.onClick_.bind(this));
      this.appendChild(appImg);
      this.appImg_ = appImg;

      var appSpan = this.ownerDocument.createElement('span');
      appSpan.textContent = this.appData.name;
      this.appendChild(appSpan);

      /* TODO(estade): grabber */
    },

    /**
     * Set the size and position of the app tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      this.appImg_.style.width = this.appImg_.style.height =
          (size * APP_IMG_SIZE_FRACTION) + 'px';
      this.style.width = this.style.height = size + 'px';
      this.style.left = x + 'px';
      this.style.top = y + 'px';
    },

    /**
     * Invoked when an app is clicked
     * @param {Event} e The click event.
     * @private
     */
    onClick_: function(e) {
      // Tell chrome to launch the app.
      var NTP_APPS_MAXIMIZED = 0;
      chrome.send('launchApp', [this.appData.id, NTP_APPS_MAXIMIZED]);

      // Don't allow the click to trigger a link or anything
      e.preventDefault();
    },
  };

  // The fraction of the app tile size that the icon uses.
  var APP_IMG_SIZE_FRACTION = 4 / 5;

  var appsPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 3,
    // The most tiles we will show in a row.
    maxColCount: 6,

    // The smallest a tile can be.
    minTileWidth: 96 / APP_IMG_SIZE_FRACTION,
    // The biggest a tile can be.
    maxTileWidth: 128 / APP_IMG_SIZE_FRACTION,
  };
  TilePage.initGridValues(appsPageGridValues);

  /**
   * Creates a new AppsPage object.
   * @param {string} name The display name for the page.
   * @constructor
   * @extends {TilePage}
   */
  function AppsPage(name) {
    var el = new TilePage(name, appsPageGridValues);
    el.__proto__ = AppsPage.prototype;
    el.initialize();

    return el;
  }

  AppsPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('apps-page');
    },

    /**
     * Creates an app DOM element and places it at the last position on the
     * page.
     * @param {Object} appData The data object that describes the app.
     */
    appendApp: function(appData) {
      this.appendTile(new App(appData));
    },
  };

  return {
    AppsPage: AppsPage,
  };
});
