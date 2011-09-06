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

      if (!cr.isMac && !cr.isChromeOS) {
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

      this.options_.disabled = !app.appData.options_url || !app.appData.enabled;
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

      var appContents = this.ownerDocument.createElement('div');
      appContents.className = 'app-contents';

      var appImgContainer = this.ownerDocument.createElement('div');
      appImgContainer.className = 'app-img-container';
      this.appImgContainer_ = appImgContainer;

      if (!this.appData_.icon_big_exists && this.appData_.icon_small_exists)
        this.useSmallIcon_ = true;

      var appImg = this.ownerDocument.createElement('img');
      // This is temporary (setIcon_/loadIcon will overwrite it) but is visible
      // before the page is shown (e.g. if switching from most visited to
      // bookmarks).
      appImg.src = 'chrome://theme/IDR_APP_DEFAULT_ICON';
      this.appImg_ = appImg;
      this.setIcon_();
      appImgContainer.appendChild(appImg);

      if (this.useSmallIcon_) {
        var imgDiv = this.ownerDocument.createElement('div');
        imgDiv.className = 'app-icon-div';
        imgDiv.appendChild(appImgContainer);
        imgDiv.addEventListener('click', this.onClick_.bind(this));
        imgDiv.title = this.appData_.name;
        this.imgDiv_ = imgDiv;
        appContents.appendChild(imgDiv);
        this.appImgContainer_.style.position = 'absolute';
        this.appImgContainer_.style.bottom = '10px';
        this.appImgContainer_.style.left = '10px';
        var stripeDiv = this.ownerDocument.createElement('div');
        stripeDiv.className = 'color-stripe';
        imgDiv.appendChild(stripeDiv);

        chrome.send('getAppIconDominantColor', [this.id]);
      } else {
        appImgContainer.addEventListener('click', this.onClick_.bind(this));
        appImgContainer.title = this.appData_.name;
        appContents.appendChild(appImgContainer);
      }

      var appSpan = this.ownerDocument.createElement('span');
      appSpan.textContent = appSpan.title = this.appData_.name;
      appSpan.addEventListener('click', this.onClick_.bind(this));
      appContents.appendChild(appSpan);
      this.appendChild(appContents);

      var notification = this.appData_.notification;
      var hasNotification = typeof notification != 'undefined' &&
                            typeof notification['title'] != 'undefined' &&
                            typeof notification['body'] != 'undefined';
      if (hasNotification)
        this.setupNotification_(notification);

      this.appContents_ = appContents;

      this.addEventListener('keydown', cr.ui.contextMenuHandler);
      this.addEventListener('keyup', cr.ui.contextMenuHandler);

      // This hack is here so that appContents.contextMenu will be the same as
      // this.contextMenu.
      var self = this;
      appContents.__defineGetter__('contextMenu', function() {
        return self.contextMenu;
      });
      appContents.addEventListener('contextmenu', cr.ui.contextMenuHandler);

      this.isStore_ = this.appData_.is_webstore;
      if (this.isStore_)
        this.createAppsPromoExtras_();

      this.addEventListener('mousedown', this.onMousedown_, true);
    },

    /**
     * Sets the color of the favicon dominant color bar.
     * @param {string} color The css-parsable value for the color.
     */
    set stripeColor(color) {
      this.querySelector('.color-stripe').style.backgroundColor = color;
    },

    /**
     * Removes the app tile from the page. Should be called after the app has
     * been uninstalled.
     */
    remove: function() {
      // Unset the ID immediately, because the app is already gone. But leave
      // the tile on the page as it animates out.
      this.id = '';
      this.tile.doRemove();
    },

    /**
     * Set the URL of the icon from |appData_|. This won't actually show the
     * icon until loadIcon() is called (for performance reasons; we don't want
     * to load icons until we have to).
     * @private
     */
    setIcon_: function() {
      var src = this.useSmallIcon_ ? this.appData_.icon_small :
                                     this.appData_.icon_big;
      if (!this.appData_.enabled ||
          (!this.appData_.offline_enabled && !navigator.onLine)) {
        src += '?grayscale=true';
      }

      this.appImgSrc_ = src;
    },

    /**
     * Shows the icon for the app. That is, it causes chrome to load the app
     * icon resource.
     */
    loadIcon: function() {
      if (this.appImgSrc_)
        this.appImg_.src = this.appImgSrc_;
      this.appImgSrc_ = null;
    },

    // Shows a notification text below the app icon and stuffs the attributes
    // necessary to show the bubble when the user clicks on the notification
    // text.
    setupNotification_: function(notification) {
      // Remove the old notification from this node (if any).
      if (this.appNotification_)
        this.appNotification_.parentNode.removeChild(this.appNotification_);

      if (notification) {
        // Add a new notification to this node.
        var appNotification = this.ownerDocument.createElement('span');
        appNotification.className = 'app-notification';
        appNotification.textContent = notification['title'];
        appNotification.addEventListener('click',
                                         this.onNotificationClick_.bind(this));
        appNotification.notificationTitle = notification['title'];
        appNotification.notificationMessage = notification['body'];
        if (typeof notification['linkUrl'] != 'undefined' &&
            typeof notification['linkText'] != 'undefined') {
          appNotification.notificationLink = notification['linkUrl'];
          appNotification.notificationLinkText = notification['linkText'];
        }
        this.appNotification_ = appNotification;
        this.appendChild(appNotification);
      }
    },

    /**
     * Creates the apps-promo section of the app (should only be called for the
     * webstore app).
     * @private
     */
    createAppsPromoExtras_: function() {
      this.classList.add('webstore');

      this.appsPromoExtras_ = $('apps-promo-extras-template').cloneNode(true);
      this.appsPromoExtras_.id = '';
      this.appsPromoHeading_ =
          this.appsPromoExtras_.querySelector('.apps-promo-heading');
      this.appsPromoLink_ =
          this.appsPromoExtras_.querySelector('.apps-promo-link');

      this.appsPromoLogo_ = this.ownerDocument.createElement('img');
      this.appsPromoLogo_.className = 'apps-promo-logo';
      this.appImgContainer_.appendChild(this.appsPromoLogo_);

      this.appendChild(this.appsPromoExtras_);
      this.appsPromoExtras_.hidden = false;
    },

    /**
     * Sets the apps promo appearance. If |data| is null, there is no promo. If
     * |data| is non-null, it contains strings to be shown for the promo. The
     * promo is only shown when the webstore app icon is alone on a page.
     * @param {Object} data A dictionary that contains apps promo strings.
     */
    setAppsPromoData: function(data) {
      if (data) {
        this.classList.add('has-promo');
      } else {
        this.classList.remove('has-promo');
        return;
      }

      this.appsPromoHeading_.textContent = data.promoHeader;
      this.appsPromoLink_.href = data.promoLink;
      this.appsPromoLink_.textContent = data.promoButton;
      this.appsPromoLogo_.src = data.promoLogo;
    },

    /**
     * Set the size and position of the app tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      var imgSize = size * APP_IMG_SIZE_FRACTION;
      this.appImgContainer_.style.width = this.appImgContainer_.style.height =
          this.useSmallIcon_ ? '16px' : imgSize + 'px';
      if (this.useSmallIcon_) {
        // 3/4 is the ratio of 96px to 128px (the used height and full height
        // of icons in apps).
        var iconSize = imgSize * 3/4;
        // The -2 is for the div border to improve the visual alignment for the
        // icon div.
        this.imgDiv_.style.width = this.imgDiv_.style.height =
            (iconSize - 2) + 'px';
        // Margins set to get the icon placement right and the text to line up.
        this.imgDiv_.style.marginTop = this.imgDiv_.style.marginBottom =
            ((imgSize - iconSize) / 2) + 'px';
      }

      this.style.width = this.style.height = size + 'px';
      if (this.isStore_)
        this.appsPromoExtras_.style.left = size + (imgSize - size) / 2 + 'px';

      this.style.left = x + 'px';
      this.style.right = x + 'px';
      this.style.top = y + 'px';
    },

    /**
     * Invoked when an app is clicked.
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
     * Invoked when an app notification is clicked. This will show the
     * notification bubble, containing the details of the notification.
     * @param {Event} e The click event.
     * @private
     */
    onNotificationClick_: function(e) {
      var title = this.appNotification_.notificationTitle;
      var message = this.appNotification_.notificationMessage;
      var link = this.appNotification_.notificationLink;
      var linkMessage = this.appNotification_.notificationLinkText;

      if (!title || !message)
        return;

      var container = this.ownerDocument.createElement('div');
      var titleItem = this.ownerDocument.createElement('strong');
      titleItem.textContent = title;
      container.appendChild(titleItem);
      var messageDiv = this.ownerDocument.createElement('div');
      messageDiv.textContent = message;
      container.appendChild(messageDiv);
      if (link && linkMessage) {
        var anchor = this.ownerDocument.createElement('a');
        anchor.href = link;
        anchor.textContent = linkMessage;
        container.appendChild(anchor);
      }

      var infoBubble = new cr.ui.Bubble;
      infoBubble.anchorNode = e.target;
      infoBubble.content = container;
      infoBubble.show();
    },

    /**
     * Handler for mousedown on the App. Adds a class that allows us to
     * not display as :active for right clicks and clicks on app notifications
     * (specifically, don't pulse on these occasions).
     * @param {Event} e The mousedown event.
     */
    onMousedown_: function(e) {
      if (e.button == 2 || e.target.classList.contains('app-notification'))
        this.classList.add('suppress-active');
      else
        this.classList.remove('suppress-active');
    },

    /**
     * Change the appData and update the appearance of the app.
     * @param {Object} appData The new data object that describes the app.
     */
    replaceAppData: function(appData) {
      this.appData_ = appData;
      this.setIcon_();
      this.loadIcon();
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

    /**
     * Returns whether this element can be 'removed' from chrome (i.e. whether
     * the user can drag it onto the trash and expect something to happen).
     * @return {boolean} True if the app can be uninstalled.
     */
    canBeRemoved: function() {
      return this.appData_.can_uninstall;
    },

    /**
     * Uninstalls the app after it's been dropped on the trash.
     */
    removeFromChrome: function() {
      chrome.send('uninstallApp', [this.appData_.id, true]);

      this.tile.tilePage.cleanupDrag();
      this.tile.parentNode.removeChild(this.tile);
    },

    /**
     * Called when a drag is starting on the tile. Updates dataTransfer with
     * data for this tile.
     */
    setDragData: function(dataTransfer) {
      dataTransfer.setData('Text', this.appData_.name);
      dataTransfer.setData('URL', this.appData_.launch_url);
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
    minTileWidth: 64 / APP_IMG_SIZE_FRACTION,
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

      this.addEventListener('cardselected', this.onCardSelected_);
    },

    /**
     * Creates an app DOM element and places it at the last position on the
     * page.
     * @param {Object} appData The data object that describes the app.
     * @param {?boolean} animate If true, the app tile plays an animation.
     */
    appendApp: function(appData, animate) {
      if (animate) {
        // Select the page and scroll all the way down so the animation is
        // visible.
        ntp4.getCardSlider().selectCardByValue(this);
        this.content_.scrollTop = this.content_.scrollHeight;
      }
      this.appendTile(new App(appData), animate);
    },

    /**
     * Handler for 'cardselected' event, fired when |this| is selected. The
     * first time this is called, we load all the app icons.
     * @private
     */
    onCardSelected_: function(e) {
      if (this.hasBeenSelected_)
        return;
      this.hasBeenSelected_ = true;

      var apps = this.querySelectorAll('.app');
      for (var i = 0; i < apps.length; i++) {
        apps[i].loadIcon();
      }
    },

    /** @inheritdoc */
    doDragOver: function(e) {
      var tile = ntp4.getCurrentlyDraggingTile();
      if (tile && !tile.querySelector('.app')) {
        e.preventDefault();
        this.setDropEffect(e.dataTransfer);
      } else {
        TilePage.prototype.doDragOver.call(this, e);
      }
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(e) {
      return ntp4.getCurrentlyDraggingTile() ||
          (e.dataTransfer && e.dataTransfer.types.indexOf('url') != -1);
    },

    /** @inheritDoc */
    addDragData: function(dataTransfer, index) {
      var currentlyDraggingTile = ntp4.getCurrentlyDraggingTile();
      if (currentlyDraggingTile) {
        var tileContents = currentlyDraggingTile.firstChild;
        if (tileContents.classList.contains('app')) {
          this.tileGrid_.insertBefore(
              currentlyDraggingTile,
              this.tileElements_[index]);
          this.tileMoved(currentlyDraggingTile);
        } else if (currentlyDraggingTile.querySelector('.most-visited')) {
          this.generateAppForLink(tileContents.data);
        }
      } else {
        this.addOutsideData_(dataTransfer, index);
      }
    },

    /**
     * Adds drag data that has been dropped from a source that is not a tile.
     * @param {Object} dataTransfer The data transfer object that holds drop
     *     data.
     * @param {number} index The index for the new data.
     * @private
     */
    addOutsideData_: function(dataTransfer, index) {
      var url = dataTransfer.getData('url');
      assert(url);

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

      // Make sure title is >=1 and <=45 characters for Chrome app limits.
      if (!title)
        title = url;
      if (title.length > 45)
        title = title.substring(0, 45);
      var data = {url: url, title: title};

      // Synthesize an app.
      this.generateAppForLink(data);
    },

    /**
     * Creates a new crx-less app manifest and installs it.
     * @param {Object} data The data object describing the link. Must have |url|
     *     and |title| members.
     * TODO(estade): pass along an index.
     */
    generateAppForLink: function(data) {
      assert(data.url != undefined);
      assert(data.title != undefined);
      var pageIndex = ntp4.getAppsPageIndex(this);
      chrome.send('generateAppForLink', [data.url, data.title, pageIndex]);
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

    /** @inheritDoc */
    setDropEffect: function(dataTransfer) {
      var tile = ntp4.getCurrentlyDraggingTile();
      if (tile && tile.querySelector('.app'))
        dataTransfer.dropEffect = 'move';
      else
        dataTransfer.dropEffect = 'copy';
    },
  };

  AppsPage.setPromo = function(data) {
    var store = document.querySelector('.webstore');
    if (store)
      store.setAppsPromoData(data);
  };

  /**
   * Callback invoked by chrome whenever an app preference changes.
   * @param {Object} data An object with all the data on available
   *     applications.
   */
  function appsPrefChangeCallback(data) {
    for (var i = 0; i < data.apps.length; ++i) {
      $(data.apps[i].id).appData = data.apps[i];
    }
  }

  /**
   * Launches the specified app using the APP_LAUNCH_NTP_APP_RE_ENABLE
   * histogram. This should only be invoked from the AppLauncherHandler.
   * @param {String} appID The ID of the app.
   */
  function launchAppAfterEnable(appId) {
    chrome.send('launchApp', [appId, APP_LAUNCH.NTP_APP_RE_ENABLE]);
  };

  function appNotificationChanged(id, notification) {
    $(id).setupNotification_(notification);
  };

  return {
    APP_LAUNCH: APP_LAUNCH,
    appNotificationChanged: appNotificationChanged,
    AppsPage: AppsPage,
    appsPrefChangeCallback: appsPrefChangeCallback,
    launchAppAfterEnable: launchAppAfterEnable,
  };
});

// TODO(estade): update the content handlers to use ntp namespace instead of
// making these global.
var appNotificationChanged = ntp4.appNotificationChanged;
var appsPrefChangeCallback = ntp4.appsPrefChangeCallback;
var launchAppAfterEnable = ntp4.launchAppAfterEnable;
