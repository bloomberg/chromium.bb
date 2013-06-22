// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('apps_dev_tool', function() {
  'use strict';

  // The list of all packed/unpacked apps and extensions.
  var completeList = [];

  // The list of all packed apps.
  var packedAppList = [];

  // The list of all packed extensions.
  var packedExtensionList = [];

  // The list of all unpacked apps or extensions.
  var unpackedList = [];

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
    var extensions = new ItemsList($('packed-extension-list'),
                                   packedExtensionList);
    var apps = new ItemsList($('packed-app-list'), packedAppList);
    var unpacked = new ItemsList($('unpacked-list'), unpackedList);
    extensions.showItemNodes();
    apps.showItemNodes();
    unpacked.showItemNodes();
  }

  /**
   * Applies the given |filter| to the items list.
   * @param {string} filter Curent string in the search box.
   */
  function rebuildAppList(filter) {
    packedAppList = [];
    packedExtensionList = [];
    unpackedList = [];

    for (var i = 0; i < completeList.length; i++) {
      var item = completeList[i];
      if (filter && item.name.toLowerCase().search(filter.toLowerCase()) < 0)
        continue;
      if (item.is_unpacked)
        unpackedList.push(item);
      else if (item.isApp)
        packedAppList.push(item);
      else
        packedExtensionList.push(item);
    }
  }

  /**
   * Create item nodes from the metadata.
   * @constructor
   */
  function ItemsList(itemsTabNode, items) {
    this.items_ = items;
    this.itemsTabNode_ = itemsTabNode;
    assert(this.itemsTabNode_);
  }

  ItemsList.prototype = {

    /**
     * |items_| holds the metadata of all apps / extensions.
     * @type {!Array.<!Object>}
     * @private
     */
    items_: [],

    /**
     * |itemsTabNode_| html element holding the items tab.
     * @type {!HTMLElement}
     * @private
     */
    itemsTabNode_: null,

    /**
     * Creates all items from scratch.
     */
    showItemNodes: function() {
      this.itemsTabNode_.textContent = '';
      // Iterate over the items and add each item to the list.
      this.itemsTabNode_.classList.toggle('empty-item-list',
                                          this.items_.length == 0);
      for (var i = 0; i < this.items_.length; ++i) {
        this.createNode_(this.items_[i]);
      }
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

      node.querySelector('.extension-disabled').hidden = item.enabled;

      if (!item.may_disable)
        node.classList.add('may-not-disable');

      var itemNode = node.querySelector('.extension-list-item');
      itemNode.style.backgroundImage = 'url(' + item.icon_url + ')';

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
      if (item.allow_reload)
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

        var launchButton = node.querySelector('.extension-run-button');
        launchButton.addEventListener('click', function(e) {
          ItemsList.launchApp(item.id);
        });

        var restartButton = node.querySelector('.extension-restart-button');
        restartButton.addEventListener('click', function(e) {
          chrome.developerPrivate.restart(item.id, function() {
            ItemsList.loadItemsInfo();
          });
        });

        var showLogs = node.querySelector('.extension-show-logs-button');
        showLogs.addEventListener('click', function(e) {
          if (!item.views.length)
            return;
          var view = item.views[0];

          // Opens the devtools inspect window for the page.
          chrome.developerPrivate.inspect({
            extension_id: String(item.id),
            render_process_id: String(view.render_process_id),
            render_view_id: String(view.render_view_id),
            incognito: view.incognito,
          });
        });
      } else {
        node.querySelector('.extension-run-button').hidden = true;
        node.querySelector('.extension-restart-button').hidden = true;
        node.querySelector('.extension-show-logs-button').hidden = true;
      }

      // The terminated reload link.
      if (!item.terminated)
        this.setEnabledCheckbox_(item, node);
      else
        this.setTerminatedReloadLink_(item, node);

      // Set remove button handler.
      this.setRemoveButton_(item, node);

      // First get the item id.
      var idLabel = node.querySelector('.extension-id');
      idLabel.textContent = ' ' + item.id;

      // Set the path and show the pack button, if provided by unpacked
      // app / extension.
      if (item.is_unpacked) {
        var loadPath = node.querySelector('.load-path');
        loadPath.hidden = false;
        loadPath.querySelector('span:nth-of-type(2)').textContent =
            ' ' + item.path;
        this.setPackButton_(item, node);
      }

      // Then the 'managed, cannot uninstall/disable' message.
      if (!item.may_disable)
        node.querySelector('.managed-message').hidden = false;

      this.setActiveViews_(item, node);

      var moreDetailsLink =
          node.querySelector('.extension-more-details-button');
      moreDetailsLink.addEventListener('click', function(e) {
        this.toggleExtensionDetails_(item, node);
      }.bind(this));

      this.itemsTabNode_.appendChild(node);
    },

    /**
     * Sets the webstore link.
     * @param {!Object} item A dictionary of item metadata.
     * @param {!HTMLElement} el HTML element containing all items.
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
     * @param {!HTMLElement} el HTML element containing all items.
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
     * @param {!HTMLElement} el HTML element containing all items.
     * @private
     */
    setTerminatedReloadLink_: function(item, el) {
      var terminatedReload = el.querySelector('.terminated-reload-link');
      terminatedReload.addEventListener('click', function(e) {
        chrome.developerPrivate.reload(item.id, function() {
          ItemsList.loadItemsInfo();
        });
      });
      terminatedReload.hidden = false;
    },

    /**
     * Sets the permissions link handler.
     * @param {!Object} item A dictionary of item metadata.
     * @param {!HTMLElement} el HTML element containing all items.
     * @private
     */
    setPermissionsLink_: function(item, el) {
      var permissions = el.querySelector('.permissions-link');
      permissions.addEventListener('click', function(e) {
        chrome.developerPrivate.showPermissionsDialog(item.id);
      });
    },

    /**
     * Sets the pack button handler.
     * @param {!Object} item A dictionary of item metadata.
     * @param {!HTMLElement} el HTML element containing all items.
     * @private
     */
    setPackButton_: function(item, el) {
      var packButton = el.querySelector('.pack-link');
      packButton.addEventListener('click', function(e) {
        $('item-root-dir').value = item.path;
        AppsDevTool.showOverlay($('packItemOverlay'));
      });
      packButton.hidden = false;
    },

    /**
     * Sets the remove button handler.
     * @param {!Object} item A dictionary of item metadata.
     * @param {!HTMLElement} el HTML element containing all items.
     * @private
     */
    setRemoveButton_: function(item, el) {
      var deleteLink = el.querySelector('.delete-link');
      deleteLink.addEventListener('click', function(e) {
        var options = {showConfirmDialog: false};
        chrome.management.uninstall(item.id, options, function() {
          ItemsList.loadItemsInfo();
        });
      });
    },

    /**
     * Sets the handler for enable checkbox.
     * @param {!Object} item A dictionary of item metadata.
     * @param {!HTMLElement} el HTML element containing all items.
     * @private
     */
    setEnabledCheckbox_: function(item, el) {
      var enable = el.querySelector('.enable-checkbox');
      enable.hidden = false;
      enable.querySelector('input').disabled = !item.may_disable;

      if (item.may_disable) {
        enable.addEventListener('click', function(e) {
          chrome.developerPrivate.enable(
              item.id, !!e.target.checked, function() {
                ItemsList.loadItemsInfo();
              });
        });
      }

      enable.querySelector('input').checked = item.enabled;
    },

    /**
     * Sets the handler for the allow_file_access checkbox.
     * @param {!Object} item A dictionary of item metadata.
     * @param {!HTMLElement} el HTML element containing all items.
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
     * @param {!HTMLElement} el HTML element containing all items.
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
     * @param {!HTMLElement} el HTML element containing all items.
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
          // Opens the devtools inspect window for the page.
          chrome.developerPrivate.inspect({
            extension_id: String(item.id),
            render_process_id: String(view.render_process_id),
            render_view_id: String(view.render_view_id),
            incognito: view.incognito,
          });
        });

        if (i < item.views.length - 1) {
          link = link.cloneNode(true);
          activeViews.appendChild(link);
        }
      });
    },

    /**
     * If the details of an app / extension is expanded, this function will
     * collapse it, else it will expand this them.
     * @param {!Object} item A dictionary of item metadata.
     * @param {!HTMLElement} node HTML element containing all items.
     * @private
     */
    toggleExtensionDetails_: function(item, node)  {
      var itemNode = node.querySelector('.extension-details');
      if (itemNode.classList.contains('expanded'))
        this.setExtensionDetailsVisible_(node, false);
      else
        this.setExtensionDetailsVisible_(node, true);
    },

    /**
     * If visible is true, this function will expand the details of an
     * app/extension, else it will collapse them.
     * @param {!HTMLElement} node HTML element containing all items.
     * @param {boolean} visible Visiblity of the details.
     * @private
     */
    setExtensionDetailsVisible_: function(node, visible) {
      var itemNode = node.querySelector('.extension-details');
      var details = node.querySelector('.extension-details-all');
      if (visible) {
        // Hide other details.
        var otherNodeList =
            document.querySelectorAll('extension-list-item-wrapper');
        for (var i = 0; i < otherNodeList.length; i++) {
          if (otherNodeList[i] != node)
            this.setExtensionDetailsVisible_(otherNodeList[i], false);
        }

        var container =
            details.querySelector('.extension-details-all-container');
        // Adds 10 pixels to height because .extension-details-all-bubble has
        // a 10px top margin.
        var height = container.clientHeight + 10;
        details.style.height = height + 'px';
        itemNode.classList.add('expanded');
      } else {
        details.style.height = 0;
        itemNode.classList.remove('expanded');
      }
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
   * @param {Function=} opt_callback An optional callback to be run when
   *     reloading is finished.
   */
  ItemsList.loadItemsInfo = function(callback) {
    chrome.developerPrivate.getItemsInfo(true, true, function(info) {
      completeList = info.sort(compareByName);
      ItemsList.onSearchInput();
      assert(/undefined|function/.test(typeof callback));
      if (callback)
        callback();
    });
  };

  /**
   * Launches the item with id |id|.
   * @param {string} id Item ID.
   */
  ItemsList.launchApp = function(id) {
    chrome.management.launchApp(id, function() {
      ItemsList.loadItemsInfo(function() {
        var unpacked = new ItemsList($('unpacked-list'), unpackedList);
        unpacked.setExtensionDetailsVisible_($(id), true);
      });
    });
  };

  /**
   * Selects the unpacked apps / extensions tab, scrolls to the app /extension
   * with the given |id| and expand its details.
   * @param {string} id Identifier of the app / extension.
   */
  ItemsList.makeUnpackedExtensionVisible = function(id) {
    var tabbox = document.querySelector('tabbox');
    // Unpacked tab is the first tab.
    tabbox.selectedIndex = 0;

    var firstItem =
        document.querySelector('#unpacked-list .extension-list-item-wrapper');
    if (!firstItem)
      return;
    // Scroll relatively to the position of the first item.
    var node = $(id);
    document.body.scrollTop = node.offsetTop - firstItem.offsetTop;
    var unpacked = new ItemsList($('unpacked-list'), unpackedList);
    unpacked.setExtensionDetailsVisible_(node, true);
  };

  return {
    ItemsList: ItemsList,
  };
});
