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

  AppsPage.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      this.className = 'apps-page';
      /* Ordered list of our apps. */
      this.appElements = this.getElementsByClassName('app');
    },

    /**
     * Creates an app DOM element and places it at the last position on the
     * page.
     * @param {Object} appData The data object that describes the app.
     */
    appendApp: function(appData) {
      var appElement = new App(appData);
      this.appendChild(appElement);
    }
  };

  return {
    AppsPage: AppsPage,
  };
});
