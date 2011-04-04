// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {

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

      this.className = 'app tile';
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

      var appSpan = this.ownerDocument.createElement('span');
      appSpan.textContent = this.appData.name;
      this.appendChild(appSpan);

      /* TODO(estade): grabber */
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

  /**
   * Creates a new AppsPage object. This object contains apps and controls
   * their layout.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function AppsPage() {
    var el = cr.doc.createElement('div');
    el.__proto__ = AppsPage.prototype;
    el.initialize();

    return el;
  }

  var MAX_ROW_TILE_COUNT = 6;
  var MIN_ROW_TILE_COUNT = 3;

  AppsPage.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      this.className = 'apps-page';
      /* Ordered list of our apps. */
      this.appElements = this.getElementsByClassName('app');

      this.lastWidth_ = this.clientWidth;

      this.eventTracker = new EventTracker();
      this.eventTracker.add(window, 'resize', this.onResize_.bind(this));
    },

    /**
     * Cleans up resources that are no longer needed after this AppsPage
     * instance is removed from the DOM.
     */
    tearDown: function() {
      this.eventTracker.removeAll();
    },

    /**
     * Creates an app DOM element and places it at the last position on the
     * page.
     * @param {Object} appData The data object that describes the app.
     */
    appendApp: function(appData) {
      var appElement = new App(appData);
      this.appendChild(appElement);
      this.positionApp_(this.appElements.length - 1);
    },

    /**
     * Calculates the x/y coordinates for an element and moves it there.
     * @param {number} The index of the element to be positioned.
     */
    positionApp_: function(index) {
      var width = this.clientWidth;
      // Vertical and horizontal spacing between tiles.
      var innerSpacing = 28;

      // Try to show 6 elements per row. If there's not enough space, go for
      // 3.
      var numRowTiles = MAX_ROW_TILE_COUNT;
      var minSixupTileSize = 128;
      var tileSize = (width - (numRowTiles - 1) * innerSpacing) / numRowTiles;
      if (tileSize < minSixupTileSize) {
        numRowTiles = MIN_ROW_TILE_COUNT;
        tileSize = minSixupTileSize;
      }

      var row = Math.floor(index / numRowTiles);
      var col = index % numRowTiles;

      var usedWidth = numRowTiles * tileSize + (numRowTiles - 1) * innerSpacing;
      var leftSpacing = (width - usedWidth) / 2;
      var x = col * (innerSpacing + tileSize) + leftSpacing;
      var topSpacing = 50;
      var y = row * (innerSpacing + tileSize) + topSpacing;

      var tileElement = this.appElements[index];
      tileElement.style.left = x + 'px';
      tileElement.style.top = y + 'px';
    },

    /**
     * Find the index within the page of the given application.
     * returns {int} The index of the app.
     */
    indexOf_: function(appElement) {
      for (var i = 0; i < this.appElements.length; i++) {
        if (appElement == this.appElements[i])
          return i;
      }

      return -1;
    },

    /**
     * Window resize event handler. Window resizes may trigger re-layouts.
     * @param {Object} e The resize event.
     */
    onResize_: function(e) {
      // Do nothing if the width didn't change.
      if (this.lastWidth_ == this.clientWidth)
        return;

      this.lastWidth_ = this.clientWidth;

      for (var i = 0; i < this.appElements.length; i++) {
        this.positionApp_(i);
      }
    },
  };

  return {
    AppsPage: AppsPage,
  };
});
