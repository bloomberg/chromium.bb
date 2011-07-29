// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var localStrings = new LocalStrings;

  var APP_LAUNCH = {
    // The histogram buckets (keep in sync with extension_constants.h).
    NTP_APPS_MAXIMIZED: 0,
    NTP_APPS_COLLAPSED: 1,
    NTP_APPS_MENU: 2,
    NTP_MOST_VISITED: 3,
    NTP_RECENTLY_CLOSED: 4,
    NTP_APP_RE_ENABLE: 16
  };

  /**
   * App context menu. The class is designed to be used as a singleton with
   * the app that is currently showing a context menu stored in this.app_.
   * @constructor
   */
  function AppContextMenu() {
    this.__proto__ = AppContextMenu.prototype;
    this.initialize();
  };
  cr.addSingletonGetter(AppContextMenu);

  AppContextMenu.prototype = {
    initialize: function() {
      var menu = new cr.ui.Menu;
      cr.ui.decorate(menu, cr.ui.Menu);
      menu.classList.add('app-context-menu');
      this.menu = menu;

      this.launch_ = this.appendMenuItem_();
      this.launch_.addEventListener('activate', this.onLaunch_.bind(this));

      menu.appendChild(cr.ui.MenuItem.createSeparator());
      this.launchRegularTab_ = this.appendMenuItem_('applaunchtyperegular');
      this.launchPinnedTab_ = this.appendMenuItem_('applaunchtypepinned');
      if (!cr.isMac)
        this.launchNewWindow_ = this.appendMenuItem_('applaunchtypewindow');
      this.launchFullscreen_ = this.appendMenuItem_('applaunchtypefullscreen');

      var self = this;
      this.forAllLaunchTypes_(function(launchTypeButton, id) {
        launchTypeButton.addEventListener('activate',
            self.onLaunchTypeChanged_.bind(self));
      });

      menu.appendChild(cr.ui.MenuItem.createSeparator());
      this.options_ = this.appendMenuItem_('appoptions');
      this.uninstall_ = this.appendMenuItem_('appuninstall');
      this.options_.addEventListener('activate',
                                     this.onShowOptions_.bind(this));
      this.uninstall_.addEventListener('activate',
                                       this.onUninstall_.bind(this));

      if (!cr.isMac && !cr.isChromeOs) {
        menu.appendChild(cr.ui.MenuItem.createSeparator());
        this.createShortcut_ = this.appendMenuItem_('appcreateshortcut');
        this.createShortcut_.addEventListener(
            'activate', this.onCreateShortcut_.bind(this));
      }

      menu.hidden = true;
      document.body.appendChild(menu);
    },

    /**
     * Appends a menu item to |this.menu|.
     * @param {?String} textId If non-null, the ID for the localized string
     *     that acts as the item's label.
     */
    appendMenuItem_: function(textId) {
      var button = cr.doc.createElement('button');
      this.menu.appendChild(button);
      cr.ui.decorate(button, cr.ui.MenuItem);
      if (textId)
        button.textContent = localStrings.getString(textId);
      return button;
    },

    /**
     * Iterates over all the launch type menu items.
     * @param {function(cr.ui.MenuItem, number)} f The function to call for each
     *     menu item. The parameters to the function include the menu item and
     *     the associated launch ID.
     */
    forAllLaunchTypes_: function(f) {
      // Order matters: index matches launchType id.
      var launchTypes = [ this.launchPinnedTab_,
                          this.launchRegularTab_,
                          this.launchFullscreen_,
                          this.launchNewWindow_ ];

      for (var i = 0; i < launchTypes.length; ++i) {
        if (!launchTypes[i])
          continue;

        f(launchTypes[i], i);
      }
    },

    /**
     * Does all the necessary setup to show the menu for the give app.
     * @param {App} app The App object that will be showing a context menu.
     */
    setupForApp: function(app) {
      this.app_ = app;

      this.launch_.textContent = app.appData.name;

      this.forAllLaunchTypes_(function(launchTypeButton, id) {
        launchTypeButton.disabled = false;
        launchTypeButton.checked = app.appData.launch_type == id;
      });

      this.options_.disabled = !app.appData.options_url;
      this.uninstall_.disabled = !app.appData.can_uninstall;
    },

    /**
     * Handlers for menu item activation.
     * @param {Event} e The activation event.
     * @private
     */
    onLaunch_: function(e) {
      chrome.send('launchApp', [this.app_.appId, APP_LAUNCH.NTP_APPS_MENU]);
    },
    onLaunchTypeChanged_: function(e) {
      var pressed = e.currentTarget;
      var app = this.app_;
      this.forAllLaunchTypes_(function(launchTypeButton, id) {
        if (launchTypeButton == pressed) {
          chrome.send('setLaunchType', [app.appId, id]);
          // Manually update the launch type. We will only get
          // appsPrefChangedCallback calls after changes to other NTP instances.
          app.appData.launch_type = id;
        }
      });
    },
    onShowOptions_: function(e) {
      window.location = this.app_.appData.options_url;
    },
    onUninstall_: function(e) {
      chrome.send('uninstallApp', [this.app_.appData.id]);
    },
    onCreateShortcut_: function(e) {
      chrome.send('createAppShortcut', [this.app_.appData.id]);
    },
  };

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
      assert(this.appData_.id, 'Got an app without an ID');
      this.id = this.appData_.id;

      this.className = 'app';

      if (!this.appData_.icon_big_exists && this.appData_.icon_small_exists)
        this.useSmallIcon_ = true;

      var appImg = this.ownerDocument.createElement('img');
      appImg.src = this.useSmallIcon_ ? this.appData_.icon_small :
                                        this.appData_.icon_big;
      // We put a click handler just on the app image - so clicking on the
      // margins between apps doesn't do anything.
      appImg.addEventListener('click', this.onClick_.bind(this));
      if (this.useSmallIcon_) {
        var imgDiv = this.ownerDocument.createElement('div');
        imgDiv.setAttribute('class', 'app-icon-div');
        imgDiv.appendChild(appImg);
        imgDiv.addEventListener('click', this.onClick_.bind(this));
        this.imgDiv_ = imgDiv;
        this.appendChild(imgDiv);
      } else {
        this.appendChild(appImg);
      }
      this.appImg_ = appImg;

      var appSpan = this.ownerDocument.createElement('span');
      appSpan.textContent = this.appData_.name;
      this.appendChild(appSpan);

      this.addEventListener('contextmenu', cr.ui.contextMenuHandler);
      this.addEventListener('keydown', cr.ui.contextMenuHandler);
      this.addEventListener('keyup', cr.ui.contextMenuHandler);
    },

    /**
     * Set the size and position of the app tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      if (this.useSmallIcon_) {
        this.imgDiv_.style.width = (size * APP_IMG_SIZE_FRACTION) + 'px';
        this.imgDiv_.style.height = (size * APP_IMG_SIZE_FRACTION - 4) + 'px';
        this.appImg_.style.width = this.appImg_.style.height = '32px';
        var margin = (size - (size * APP_IMG_SIZE_FRACTION)) / 2;
        this.imgDiv_.style['margin-left'] = margin + 'px';
        this.imgDiv_.style['margin-right'] = margin + 'px';
        this.imgDiv_.style['margin-bottom'] = '4px';
      }
      else {
        this.appImg_.style.width = this.appImg_.style.height =
            (size * APP_IMG_SIZE_FRACTION) + 'px';
      }

      this.style.width = this.style.height = size + 'px';

      this.style.left = x + 'px';
      this.style.right = x + 'px';
      this.style.top = y + 'px';
    },

    /**
     * Invoked when an app is clicked
     * @param {Event} e The click event.
     * @private
     */
    onClick_: function(e) {
      chrome.send('launchApp',
                  [this.appId, APP_LAUNCH.NTP_APPS_MAXIMIZED,
                   e.altKey, e.ctrlKey, e.metaKey, e.shiftKey, e.button]);

      // Don't allow the click to trigger a link or anything
      e.preventDefault();
    },

    /**
     * The data and preferences for this app.
     * @type {Object}
     */
    set appData(data) {
      this.appData_ = data;
    },
    get appData() {
      return this.appData_;
    },

    get appId() {
      return this.appData_.id;
    },

    /**
     * Returns a pointer to the context menu for this app. All apps share the
     * singleton AppContextMenu. This function is called by the
     * ContextMenuHandler in response to the 'contextmenu' event.
     * @type {cr.ui.Menu}
     */
    get contextMenu() {
      var menu = AppContextMenu.getInstance();
      menu.setupForApp(this);
      return menu.menu;
    },
  };

  var TilePage = ntp4.TilePage;

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
   * @constructor
   * @extends {TilePage}
   */
  function AppsPage() {
    var el = new TilePage(appsPageGridValues);
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
     * @param {?boolean} animate If true, the app tile plays an animation.
     */
    appendApp: function(appData, animate) {
      this.appendTile(new App(appData), animate);
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(e) {
      return ntp4.getCurrentlyDraggingTile() ||
          (e.dataTransfer && e.dataTransfer.types.indexOf('url') != -1);
    },

    /** @inheritDoc */
    addOutsideData: function(dataTransfer, index) {
      var url = dataTransfer.getData('url');
      assert(url);
      if (!url)
        return;

      // If the dataTransfer has html data, use that html's text contents as the
      // title of the new link.
      var html = dataTransfer.getData('text/html');
      var title;
      if (html) {
        // It's important that we don't attach this node to the document
        // because it might contain scripts.
        var node = this.ownerDocument.createElement('div');
        node.innerHTML = html;
        title = node.textContent;
      }
      if (!title)
        title = url;

      // Synthesize an app.
      var data = {url: url, title: title};
      // Make sure title is >=1 and <=45 characters for Chrome app limits.
      if (data.title.length > 45)
        data.title = data.title.substring(0,45);
      if (data.title.length == 0)
        data.title = data.url;
      this.generateAppForLink(data);
    },

    /**
     * Creates a new crx-less app manifest and installs it.
     * @param {Object} data The data object describing the link. Must have |url|
     * and |title| members.
     */
    generateAppForLink: function(data) {
      chrome.send('generateAppForLink', [data.url, data.title]);
    },

    /** @inheritDoc */
    tileMoved: function(draggedTile) {
      if (!(draggedTile.firstChild instanceof App))
        return;

      var pageIndex = ntp4.getAppsPageIndex(this);
      chrome.send('setPageIndex', [draggedTile.firstChild.appId, pageIndex]);

      var appIds = [];
      for (var i = 0; i < this.tileElements_.length; i++) {
        var tileContents = this.tileElements_[i].firstChild;
        if (tileContents instanceof App)
          appIds.push(tileContents.appId);
      }

      chrome.send('reorderApps', [draggedTile.firstChild.appId, appIds]);
    },
  };

  return {
    AppsPage: AppsPage,
  };
});
