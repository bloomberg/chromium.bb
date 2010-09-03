// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getAppsCallback(data) {
  logEvent('recieved apps');
  var appsSection = $('apps');
  var appsSectionContent = $('apps-maxiview');
  var appsMiniview = appsSection.getElementsByClassName('miniview')[0];
  appsSectionContent.textContent = '';
  appsMiniview.textContent = '';

  if (data.apps.length == 0) {
    appsSection.classList.add('disabled');
    return;
  }

  data.apps.forEach(function(app) {
    appsSectionContent.appendChild(apps.createElement(app));
  });

  appsSectionContent.appendChild(apps.createWebStoreElement());

  data.apps.slice(0, MAX_MINIVIEW_ITEMS).forEach(function(app) {
    appsMiniview.appendChild(apps.createMiniviewElement(app));
  });

  appsSection.classList.remove('disabled');
  updateMiniviewClipping(appsMiniview);
  layoutSections();
}

var apps = {
  /**
   * @this {!HTMLAnchorElement}
   */
  handleClick_: function() {
    // TODO(arv): Handle zoom?
    var rect = this.getBoundingClientRect();
    var cs = getComputedStyle(this);
    var size = cs.backgroundSize.split(/\s+/);  // background-size has the
                                                // format '123px 456px'.
    var width = parseInt(size[0], 10);
    var height = parseInt(size[1], 10);
    // We are using background-position-x 50%.
    var left = rect.left + ((rect.width - width) >> 1);  // Integer divide by 2.
    var top = rect.top + parseInt(cs.backgroundPositionY, 10);

    chrome.send('launchApp', [this.getAttribute("app_id"),
                              String(left), String(top),
                              String(width), String(height)]);
    return false;
  },

  createElement_: function(app) {
    var div = document.createElement('div');
    div.className = 'app';

    var front = div.appendChild(document.createElement('div'));
    front.className = 'front';

    var a = front.appendChild(document.createElement('a'));
    a.setAttribute('app_id', app['id']);
    a.xtitle = a.textContent = app['name'];
    a.href = app['launch_url'];

    return div;
  },

  createElement: function(app) {
    var div = this.createElement_(app);
    var front = div.firstChild;
    var a = front.firstChild;

    a.onclick = apps.handleClick_;
    a.style.backgroundImage = url(app['icon_big']);
    if (hashParams['app-id'] == app['id']) {
      div.setAttribute('new', 'new');
      // Delay changing the attribute a bit to let the page settle down a bit.
      setTimeout(function() {
        div.setAttribute('new', 'installed');
      }, 500);
    }

    var settingsButton = front.appendChild(document.createElement('button'));
    settingsButton.className = 'flip';
    settingsButton.title = localStrings.getString('appsettings');

    var back = div.appendChild(document.createElement('div'));
    back.className = 'back';

    var header = back.appendChild(document.createElement('h2'));
    header.textContent = app['name'];

    var optionsButton = back.appendChild(document.createElement('button'));
    optionsButton.textContent = localStrings.getString('appoptions');
    optionsButton.disabled = !app['options_url'];
    optionsButton.onclick = function() {
      window.location = app['options_url'];
    };

    var uninstallButton = back.appendChild(document.createElement('button'));
    uninstallButton.textContent = uninstallButton.xtitle =
        localStrings.getString('appuninstall');
    uninstallButton.onclick = function() {
      chrome.send('uninstallApp', [app['id']]);
    };

    var closeButton = back.appendChild(document.createElement('button'));
    closeButton.title = localStrings.getString('close');
    closeButton.className = 'flip';
    closeButton.onclick = settingsButton.onclick = function() {
      div.classList.toggle('config');
    };

    return div;
  },

  createMiniviewElement: function(app) {
    var span = document.createElement('span');
    var a = span.appendChild(document.createElement('a'));

    a.setAttribute('app_id', app['id']);
    a.textContent = app['name'];
    a.href = app['launch_url'];
    a.onclick = apps.handleClick_;
    a.style.backgroundImage = url(app['icon_small']);
    a.className = 'item';
    span.appendChild(a);
    return span;
  },

  createWebStoreElement: function() {
    return this.createElement_({
      'id': 'web-store-entry',
      'name': localStrings.getString('web_store_title'),
      'launch_url': localStrings.getString('web_store_url')
    });
  }
};
