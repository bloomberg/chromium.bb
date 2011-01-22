// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var MAX_APPS_PER_ROW = [];
MAX_APPS_PER_ROW[LayoutMode.SMALL] = 4;
MAX_APPS_PER_ROW[LayoutMode.NORMAL] = 6;

// The URL prefix used in the app link 'ping' attributes.
var PING_APP_LAUNCH_PREFIX = 'record-app-launch';

// The URL prefix used in the webstore link 'ping' attributes.
var PING_WEBSTORE_LAUNCH_PREFIX = 'record-webstore-launch';

function getAppsCallback(data) {
  logEvent('received apps');

  // In the case of prefchange-triggered updates, we don't receive this flag.
  // Just leave it set as it was before in that case.
  if ('showPromo' in data)
    apps.showPromo = data.showPromo;

  var appsSection = $('apps');
  var appsSectionContent = $('apps-content');
  var appsMiniview = appsSection.getElementsByClassName('miniview')[0];
  var appsPromo = $('apps-promo');
  var appsPromoPing = PING_WEBSTORE_LAUNCH_PREFIX + '+' + apps.showPromo;
  var webStoreEntry, webStoreMiniEntry;

  // Hide menu options that are not supported on the OS or windowing system.

  // The "Launch as Window" menu option.
  $('apps-launch-type-window-menu-item').style.display =
      (data.disableAppWindowLaunch ? 'none' : 'inline');

  // The "Create App Shortcut" menu option.
  $('apps-create-shortcut-command-menu-item').style.display =
      $('apps-create-shortcut-command-separator').style.display =
      (data.disableCreateAppShortcut ? 'none' : '');

  appsMiniview.textContent = '';
  appsSectionContent.textContent = '';

  data.apps.sort(function(a,b) {
    return a.app_launch_index - b.app_launch_index;
  });

  clearClosedMenu(apps.menu);
  data.apps.forEach(function(app) {
    appsSectionContent.appendChild(apps.createElement(app));
  });

  webStoreEntry = apps.createWebStoreElement();
  webStoreEntry.querySelector('a').setAttribute('ping', appsPromoPing);
  appsSectionContent.appendChild(webStoreEntry);

  data.apps.slice(0, MAX_MINIVIEW_ITEMS).forEach(function(app) {
    appsMiniview.appendChild(apps.createMiniviewElement(app));
    addClosedMenuEntryWithLink(apps.menu, apps.createClosedMenuElement(app));
  });
  if (data.apps.length < MAX_MINIVIEW_ITEMS) {
    webStoreMiniEntry = apps.createWebStoreMiniElement();
    webStoreEntry.querySelector('a').setAttribute('ping', appsPromoPing);
    appsMiniview.appendChild(webStoreMiniEntry);
    addClosedMenuEntryWithLink(apps.menu,
                               apps.createWebStoreClosedMenuElement());
  }

  if (!data.showLauncher)
    appsSection.classList.add('disabled');
  else
    appsSection.classList.remove('disabled');

  addClosedMenuFooter(apps.menu, 'apps', MENU_APPS, Section.APPS);

  apps.loaded = true;
  if (apps.showPromo)
    document.documentElement.classList.add('apps-promo-visible');
  else
    document.documentElement.classList.remove('apps-promo-visible');

  var appsPromoLink = $('apps-promo-link');
  if (appsPromoLink)
    appsPromoLink.setAttribute('ping', appsPromoPing);
  maybeDoneLoading();

  if (isDoneLoading()) {
    if (!apps.showPromo && data.apps.length >= MAX_APPS_PER_ROW[layoutMode])
      webStoreEntry.classList.add('loner');
    else
      webStoreEntry.classList.remove('loner');

    updateMiniviewClipping(appsMiniview);
    layoutSections();
  }
}

function appsPrefChangeCallback(data) {
  // Currently the only pref that is watched is the launch type.
  data.apps.forEach(function(app) {
    var appLink = document.querySelector('.app a[app-id=' + app['id'] + ']');
    if (appLink)
      appLink.setAttribute('launch-type', app['launch_type']);
  });
}

var apps = (function() {

  function createElement(app) {
    var div = document.createElement('div');
    div.className = 'app';

    var a = div.appendChild(document.createElement('a'));
    a.setAttribute('app-id', app['id']);
    a.setAttribute('launch-type', app['launch_type']);
    a.xtitle = a.textContent = app['name'];
    a.href = app['launch_url'];

    return div;
  }

  function launchApp(appId) {
    var appsSection = $('apps');
    var expanded = !appsSection.classList.contains('collapsed');
    var element = document.querySelector(
        (expanded ? '.maxiview' : '.miniview') + ' a[app-id=' + appId + ']');

    // TODO(arv): Handle zoom?
    var rect = element.getBoundingClientRect();
    var cs = getComputedStyle(element);
    var size = cs.backgroundSize.split(/\s+/);  // background-size has the
                                                // format '123px 456px'.

    var width = parseInt(size[0], 10);
    var height = parseInt(size[1], 10);

    var top, left;
    if (expanded) {
      // We are using background-position-x 50%.
      top = rect.top + parseInt(cs.backgroundPositionY, 10);
      left = rect.left + ((rect.width - width) >> 1);  // Integer divide by 2.

    } else {
      // We are using background-position-y 50%.
      top = rect.top + ((rect.height - width) >> 1);  // Integer divide by 2.
      if (getComputedStyle(element).direction == 'rtl')
        left = rect.left + rect.width - width;
      else
        left = rect.left;
    }

    chrome.send('launchApp', [appId,
                              String(left), String(top),
                              String(width), String(height)]);
  }

  /**
   * @this {!HTMLAnchorElement}
   */
  function handleClick(e) {
    var appId = e.currentTarget.getAttribute('app-id');
    launchApp(appId);
    return false;
  }

  // Keep in sync with LaunchType in extension_prefs.h
  var LaunchType = {
    LAUNCH_PINNED: 0,
    LAUNCH_REGULAR: 1,
    LAUNCH_FULLSCREEN: 2,
    LAUNCH_WINDOW: 3
  };

  // Keep in sync with LaunchContainer in extension_constants.h
  var LaunchContainer = {
    LAUNCH_WINDOW: 0,
    LAUNCH_PANEL: 1,
    LAUNCH_TAB: 2
  };

  var currentApp;

  function addContextMenu(el, app) {
    el.addEventListener('contextmenu', cr.ui.contextMenuHandler);
    el.addEventListener('keydown', cr.ui.contextMenuHandler);
    el.addEventListener('keyup', cr.ui.contextMenuHandler);

    Object.defineProperty(el, 'contextMenu', {
      get: function() {
        currentApp = app;

        $('apps-launch-command').label = app['name'];
        $('apps-options-command').canExecuteChange();

        var launchTypeEl;
        if (el.getAttribute('app-id') === app['id']) {
          launchTypeEl = el;
        } else {
          appLinkSel = 'a[app-id=' + app['id'] + ']';
          launchTypeEl = el.querySelector(appLinkSel);
        }

        var launchType = launchTypeEl.getAttribute('launch-type');
        var launchContainer = app['launch_container'];
        var isPanel = launchContainer == LaunchContainer.LAUNCH_PANEL;

        // Update the commands related to the launch type.
        var launchTypeIds = ['apps-launch-type-pinned',
                             'apps-launch-type-regular',
                             'apps-launch-type-fullscreen',
                             'apps-launch-type-window'];
        launchTypeIds.forEach(function(id) {
          var command = $(id);
          command.disabled = isPanel;
          command.checked = !isPanel &&
              launchType == command.getAttribute('launch-type');
        });

        return $('app-context-menu');
      }
    });
  }

  document.addEventListener('command', function(e) {
    if (!currentApp)
      return;

    var commandId = e.command.id;
    switch (commandId) {
      case 'apps-options-command':
        window.location = currentApp['options_url'];
        break;
      case 'apps-launch-command':
        launchApp(currentApp['id']);
        break;
      case 'apps-uninstall-command':
        chrome.send('uninstallApp', [currentApp['id']]);
        break;
      case 'apps-create-shortcut-command':
        chrome.send('createAppShortcut', [currentApp['id']]);
        break;
      case 'apps-launch-type-pinned':
      case 'apps-launch-type-regular':
      case 'apps-launch-type-fullscreen':
      case 'apps-launch-type-window':
        chrome.send('setLaunchType',
            [currentApp['id'], e.command.getAttribute('launch-type')]);
        break;
    }
  });

  document.addEventListener('canExecute', function(e) {
    switch (e.command.id) {
      case 'apps-options-command':
        e.canExecute = currentApp && currentApp['options_url'];
        break;
      case 'apps-launch-command':
      case 'apps-uninstall-command':
        e.canExecute = true;
        break;
    }
  });

  return {
    loaded: false,

    menu: $('apps-menu'),

    showPromo: false,

    createElement: function(app) {
      var div = createElement(app);
      var a = div.firstChild;

      a.onclick = handleClick;
      a.setAttribute('ping', PING_APP_LAUNCH_PREFIX + '+' + this.showPromo);
      a.style.backgroundImage = url(app['icon_big']);
      if (hashParams['app-id'] == app['id']) {
        div.setAttribute('new', 'new');
        // Delay changing the attribute a bit to let the page settle down a bit.
        setTimeout(function() {
          // Make sure the new icon is scrolled into view.
          document.body.scrollTop = document.body.scrollHeight;

          // This will trigger the 'bounce' animation defined in apps.css.
          div.setAttribute('new', 'installed');
        }, 500);
        div.addEventListener('webkitAnimationEnd', function(e) {
          div.removeAttribute('new');

          // If we get new data (eg because something installs in another tab,
          // or because we uninstall something here), don't run the install
          // animation again.
          document.documentElement.setAttribute("install-animation-enabled",
                                                "false");
        });
      }

      var settingsButton = div.appendChild(new cr.ui.ContextMenuButton);
      settingsButton.className = 'app-settings';
      settingsButton.title = localStrings.getString('appsettings');

      addContextMenu(div, app);

      return div;
    },

    createMiniviewElement: function(app) {
      var span = document.createElement('span');
      var a = span.appendChild(document.createElement('a'));

      a.setAttribute('app-id', app['id']);
      a.textContent = app['name'];
      a.href = app['launch_url'];
      a.onclick = handleClick;
      a.setAttribute('ping', PING_APP_LAUNCH_PREFIX + '+' + this.showPromo);
      a.style.backgroundImage = url(app['icon_small']);
      a.className = 'item';
      span.appendChild(a);

      addContextMenu(span, app);

      return span;
    },

    createClosedMenuElement: function(app) {
      var a = document.createElement('a');
      a.setAttribute('app-id', app['id']);
      a.textContent = app['name'];
      a.href = app['launch_url'];
      a.onclick = handleClick;
      a.setAttribute('ping', PING_APP_LAUNCH_PREFIX + '+' + this.showPromo);
      a.style.backgroundImage = url(app['icon_small']);
      a.className = 'item';

      addContextMenu(a, app);

      return a;
    },

    createWebStoreElement: function() {
      var elm = createElement({
        'id': 'web-store-entry',
        'name': localStrings.getString('web_store_title'),
        'launch_url': localStrings.getString('web_store_url')
      });
      elm.setAttribute('app-id', 'web-store-entry');
      return elm;
    },

    createWebStoreMiniElement: function() {
      var span = document.createElement('span');
      span.appendChild(this.createWebStoreClosedMenuElement());
      return span;
    },

    createWebStoreClosedMenuElement: function() {
      var a = document.createElement('a');
      a.textContent = localStrings.getString('web_store_title');
      a.href = localStrings.getString('web_store_url');
      a.style.backgroundImage = url('chrome://theme/IDR_PRODUCT_LOGO_16');
      a.className = 'item';
      return a;
    }
  };
})();
