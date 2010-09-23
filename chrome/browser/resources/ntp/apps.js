// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getAppsCallback(data) {
  logEvent('received apps');
  var appsSection = $('apps');
  var appsSectionContent = $('apps-maxiview');
  var appsMiniview = appsSection.getElementsByClassName('miniview')[0];
  appsSectionContent.textContent = '';
  appsMiniview.textContent = '';

  if (data.apps.length == 0) {
    appsSection.classList.add('disabled');
    setShownSections(Section.THUMB);
  } else {
    data.apps.forEach(function(app) {
        appsSectionContent.appendChild(apps.createElement(app));
    });

    appsSectionContent.appendChild(apps.createWebStoreElement());

    data.apps.slice(0, MAX_MINIVIEW_ITEMS).forEach(function(app) {
      appsMiniview.appendChild(apps.createMiniviewElement(app));
    });

    appsSection.classList.remove('disabled');
  }

  apps.loaded = true;
  maybeDoneLoading();

  if (data.apps.length > 0 && isDoneLoading()) {
    updateMiniviewClipping(appsMiniview);
    layoutSections();
  }
}

var apps = (function() {

  function createElement(app) {
    var div = document.createElement('div');
    div.className = 'app';

    var a = div.appendChild(document.createElement('a'));
    a.setAttribute('app-id', app['id']);
    a.xtitle = a.textContent = app['name'];
    a.href = app['launch_url'];

    return div;
  }

  function createContextMenu(app) {
    var menu = new cr.ui.Menu;
    var button = document.createElement(button);
  }

  function launchApp(appId) {
    var appsSection = $('apps');
    var expanded = !appsSection.classList.contains('hidden');
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

        return $('app-context-menu');
      }
    });
  }

  document.addEventListener('command', function(e) {
    if (!currentApp)
      return;

    switch (e.command.id) {
      case 'apps-options-command':
        window.location = currentApp['options_url'];
        break;
      case 'apps-launch-command':
        launchApp(currentApp['id']);
        break;
      case 'apps-uninstall-command':
        chrome.send('uninstallApp', [currentApp['id']]);
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

    createElement: function(app) {
      var div = createElement(app);
      var a = div.firstChild;

      a.onclick = handleClick;
      a.style.backgroundImage = url(app['icon_big']);
      if (hashParams['app-id'] == app['id']) {
        div.setAttribute('new', 'new');
        // Delay changing the attribute a bit to let the page settle down a bit.
        setTimeout(function() {
          // This will trigger the 'bounce' animation defined in apps.css.
          div.setAttribute('new', 'installed');
        }, 500);
        div.addEventListener('webkitAnimationEnd', function(e) {
          div.removeAttribute('new');
        });
        if ($('apps').classList.contains('hidden'))
          toggleSectionVisibilityAndAnimate('APPS');
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
      a.style.backgroundImage = url(app['icon_small']);
      a.className = 'item';
      span.appendChild(a);

      addContextMenu(span, app);

      return span;
    },

    createWebStoreElement: function() {
      return createElement({
        'id': 'web-store-entry',
        'name': localStrings.getString('web_store_title'),
        'launch_url': localStrings.getString('web_store_url')
      });
    }
  };
})();
