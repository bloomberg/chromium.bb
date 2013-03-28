// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('apps_dev_tool', function() {
  'use strict';

  /**
   * Creates a new list of items.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   */
  var ItemsList = cr.ui.define('div');

  // The list of all apps & extensions.
  var completeList = [];

  // The list of all apps.
  var appList = [];

  /** const*/ var AppsDevTool = apps_dev_tool.AppsDevTool;

  /**
   * @param {string} a first string.
   * @param {string} b second string.
   * @return {number} 1, 0, -1 if |a| is lexicographically greater, equal or
   *     lesser than |b| respectively.
   */
  function compare(a, b) {
    return a > b ? 1 : (a == b ? 0 : -1);
  }

  /**
   * Returns a translated string.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivalent to localStrings.getString(id).
   *
   * @param {string} id The id of the string to return.
   * @return {string} The translated string.
   */
  function str(id) {
    return loadTimeData.getString(id);
  }

  /**
   * compares strings |app1| and |app2| (case insensitive).
   * @param {string} app1 first app_name.
   * @param {string} app2 second app_name.
   */
  function compareByName(app1, app2) {
    return compare(app1.name.toLowerCase(), app2.name.toLowerCase());
  }

  /**
   * Refreshes the app.
   */
  function reloadAppDisplay() {
    var itemsDiv = $('items');

    // Empty the current content.
    itemsDiv.textContent = '';

    ItemsList.prototype.data_ = appList;
    var itemsList = $('extension-settings-list');
    ItemsList.decorate(itemsList);
  }

  /**
   * Applies the given |filter| to the items list.
   * @param {string} filter Curent string in the search box.
   */
  function rebuildAppList(filter) {
    if (!filter) {
      appList = completeList;
      return;
    }

    appList = [];
    for (var i = 0; i < completeList.length; i++) {
      var item = completeList[i];
      if (item.name.toLowerCase().search(filter) < 0)
        continue;

      appList.push(item);
    }
  }

  ItemsList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * |data_| holds the metadata of all the apps and extensions.
     * @type {!Array.<!Object>}
     * @private
     */
    data_: [],

    /**
     * @override
     */
    decorate: function() {
      this.textContent = '';
      this.showItemNodes_();
    },

    /**
     * Creates all items from scratch.
     * @private
     */
    showItemNodes_: function() {
      // Iterate over the item data and add each item to the list.
      this.classList.toggle('empty-extension-list', this.data_.length == 0);
      this.data_.forEach(this.createNode_, this);
    },

    /**
     * Synthesizes and initializes an HTML element for the item metadata
     * given in |item|.
     * @param {!Object} item A dictionary of item metadata.
     * @private
     */
    createNode_: function(item) {
      var template = $('template-collection').querySelector(
          '.extension-list-item-wrapper');
      var node = template.cloneNode(true);
      node.id = item.id;

      if (!item.enabled)
        node.classList.add('inactive-extension');

      if (!item.may_disable)
        node.classList.add('may-not-disable');

      var itemNode = node.querySelector('.extension-list-item');
      itemNode.style.backgroundImage = 'url(' + item.icon + ')';

      var title = node.querySelector('.extension-title');
      title.textContent = item.name;
      title.onclick = function() {
        if (item.isApp)
          ItemsList.launchApp(item.id);
      };

      var version = node.querySelector('.extension-version');
      version.textContent = item.version;

      var description = node.querySelector('.extension-description span');
      description.textContent = item.description;

      // The 'allow in incognito' checkbox.
      this.setAllowIncognitoCheckbox_(item, node);

      // The 'allow file:// access' checkbox.
      if (item.wants_file_access)
        this.setAllowFileAccessCheckbox_(item, node);

      // The 'Options' checkbox.
      if (item.enabled && item.options_url) {
        var options = node.querySelector('.options-link');
        options.href = item.options_url;
        options.hidden = false;
      }

      // The 'Permissions' link.
      this.setPermissionsLink_(item, node);

      // The 'View in Web Store/View Web Site' link.
      if (item.homepage_url)
        this.setWebstoreLink_(item, node);

      // The 'Reload' checkbox.
      if (item.allow_reload) {
        this.setReloadLink_(item, node);

        if (item.type == 'packaged_app') {
          // The 'Launch' link.
          var launch = node.querySelector('.launch-link');
          launch.addEventListener('click', function(e) {
            ItemsList.launchApp(item.id);
          });
          launch.hidden = false;

          // The 'Restart' link.
          var restart = node.querySelector('.restart-link');
          restart.addEventListener('click', function(e) {
            chrome.developerPrivate.restart(item.id, function() {
              ItemsList.loadItemsInfo();
            });
          });
          restart.hidden = false;
        }
      }

      // The terminated reload link.
      if (!item.terminated)
        this.setEnabledCheckbox_(item, node);
      else
        this.setTerminatedReloadLink_(node, item);

      // Set remove button handler.
      this.setRemoveButton_(item, node);

      // First get the item id.
      var idLabel = node.querySelector('.extension-id');
      idLabel.textContent = ' ' + item.id;

      // Then the path, if provided by unpacked app / extension.
      if (item.is_unpacked) {
        var loadPath = node.querySelector('.load-path');
        loadPath.hidden = false;
        loadPath.querySelector('span:nth-of-type(2)').textContent =
            ' ' + item.path;
      }

      // Then the 'managed, cannot uninstall/disable' message.
      if (!item.may_disable)
        node.querySelector('.managed-message').hidden = false;

      this.setActiveViews_(item, node);

      this.appendChild(node);
    },

    /**
     * Sets the webstore link.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setWebstoreLink_: function(item, el) {
      var siteLink = el.querySelector('.site-link');
      siteLink.href = item.homepage_url;
      siteLink.textContent = str(item.homepageProvided ?
         'extensionSettingsVisitWebsite' : 'extensionSettingsVisitWebStore');
      siteLink.hidden = false;
      siteLink.target = '_blank';
    },

    /**
     * Sets the reload link handler.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setReloadLink_: function(item, el) {
      var reload = el.querySelector('.reload-link');
      reload.addEventListener('click', function(e) {
        chrome.developerPrivate.reload(item.id, function() {
          ItemsList.loadItemsInfo();
        });
      });
      reload.hidden = false;
    },

    /**
     * Sets the terminated reload link handler.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setTerminatedReloadLink_: function(item, el) {
      var terminatedReload = el.querySelector('.terminated-reload-link');
      terminatedReload.hidden = false;
      chrome.developerPrivate.reload(item.id, function() {
        ItemsList.loadItemsInfo();
      });
    },

    /**
     * Sets the permissions link handler.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setPermissionsLink_: function(item, el) {
      var permissions = el.querySelector('.permissions-link');
      permissions.addEventListener('click', function(e) {
        var permissionItem = $('permissions-item');
        permissionItem.textContent = '';
        chrome.management.getPermissionWarningsById(
            item.id,
            function(warnings) {
              warnings.forEach(function(permission) {
                var li = document.createElement('li');
                li.textContent = permission;
                permissionItem.appendChild(li);
            });
          AppsDevTool.showOverlay($('permissions-overlay'));
        });

        $('permissions-icon').style.backgroundImage =
            'url(' + item.icon + ')';
        $('permissions-title').textContent = item.name;
        e.preventDefault();
      });
    },

    /**
     * Sets the remove button handler.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setRemoveButton_: function(item, el) {
      var trashTemplate = $('template-collection').querySelector('.trash');
      var trash = trashTemplate.cloneNode(true);
      trash.title = str('extensionUninstall');
      trash.addEventListener('click', function(e) {
        var options = {showConfirmDialog: false};
        chrome.management.uninstall(item.id, options, function() {
          ItemsList.loadItemsInfo();
        });
      });
      el.querySelector('.enable-controls').appendChild(trash);
    },

    /**
     * Sets the handler for enable checkbox.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setEnabledCheckbox_: function(item, el) {
      var enable = el.querySelector('.enable-checkbox');
      enable.hidden = false;
      enable.querySelector('input').disabled = !item.may_disable;

      if (item.may_disable) {
        enable.addEventListener('click', function(e) {
          chrome.developerPrivate.enable(
              item.id, !!e.target.checked, ItemsList.loadItemsInfo);
        });
      }

      enable.querySelector('input').checked = item.enabled;
    },

    /**
     * Sets the handler for the allow_file_access checkbox.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setAllowFileAccessCheckbox_: function(item, el) {
      var fileAccess = el.querySelector('.file-access-control');
      fileAccess.addEventListener('click', function(e) {
        chrome.developerPrivate.allowFileAccess(item.id, !!e.target.checked);
      });
      fileAccess.querySelector('input').checked = item.allow_file_access;
      fileAccess.hidden = false;
    },

    /**
     * Sets the handler for the allow_incognito checkbox.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setAllowIncognitoCheckbox_: function(item, el) {
      if (item.allow_incognito) {
        var incognito = el.querySelector('.incognito-control');
        incognito.addEventListener('change', function(e) {
          chrome.developerPrivate.allowIncognito(
              item.id, !!e.target.checked, function() {
            ItemsList.loadItemsInfo();
          });
        });
        incognito.querySelector('input').checked = item.incognito_enabled;
        incognito.hidden = false;
      }
    },

    /**
     * Sets the active views link of an item. Clicking on the link
     * opens devtools window to inspect.
     * @param {!Object} item A dictionary of item metadata.
     * @param {HTMLElement} el HTML element containing all items.
     * @private
     */
    setActiveViews_: function(item, el) {
      if (!item.views.length)
        return;

      var activeViews = el.querySelector('.active-views');
      activeViews.hidden = false;
      var link = activeViews.querySelector('a');

      item.views.forEach(function(view, i) {
        var label = view.path +
            (view.incognito ? ' ' + str('viewIncognito') : '') +
            (view.render_process_id == -1 ? ' ' + str('viewInactive') : '');
        link.textContent = label;
        link.addEventListener('click', function(e) {
          var inspectOptions = {
            extension_id: String(item.id),
            render_process_id: String(view.render_process_id),
            render_view_id: String(view.render_view_id),
            incognito: view.incognito,
          };

          // Opens the devtools inspect window for the page.
          chrome.developerPrivate.inspect(inspectOptions);
        });

        if (i < item.views.length - 1) {
          link = link.cloneNode(true);
          activeViews.appendChild(link);
        }
      });
    }
  };

  /**
   * Rebuilds the item list and reloads the app on every search input.
   */
  ItemsList.onSearchInput = function() {
    rebuildAppList($('search').value);
    reloadAppDisplay();
  };

  /**
   * Fetches items info and reloads the app.
   */
  ItemsList.loadItemsInfo = function() {
    chrome.developerPrivate.getItemsInfo(true, true, function(info) {
      completeList = info.sort(compareByName);
      ItemsList.onSearchInput();
    });
  };

  /**
   * Launches the item with id |id|.
   * @param {string} id Item ID.
   */
  ItemsList.launchApp = function(id) {
    chrome.management.launchApp(id, function() {
      // There is a delay in generation of background page for the app.
      setTimeout(ItemsList.loadItemsInfo, 1000);
    });
  };

  return {
    ItemsList: ItemsList,
  };
});
