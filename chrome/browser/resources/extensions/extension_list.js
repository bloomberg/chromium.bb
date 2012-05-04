// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  'use strict';

  /**
   * A lookup helper function to find the first node that has an id (starting
   * at |node| and going up the parent chain).
   * @param {Element} node The node to start looking at.
   */
  function findIdNode(node) {
    while (node && !node.id) {
      node = node.parentNode;
    }
    return node;
  }

  /**
   * Creates a new list of extensions.
   * @param {Object=} opt_propertyBag Optional properties.
   * @constructor
   * @extends {cr.ui.div}
   */
  var ExtensionsList = cr.ui.define('div');

  /**
   * @type {Object.<string, boolean>} A map from extension id to a boolean
   *     indicating whether the incognito warning is showing. This persists
   *     between calls to decorate.
   */
  var butterBarVisibility = {};

  ExtensionsList.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.textContent = '';

      this.showExtensionNodes_();
    },

    /**
     * Creates all extension items from scratch.
     * @private
     */
     showExtensionNodes_: function() {
       // Iterate over the extension data and add each item to the list.
       var list = this;
       this.data_.extensions.forEach(this.createNode_.bind(this));

       if (this.data_.extensions.length == 0)
         this.classList.add('empty-extension-list');
       else
         this.classList.remove('empty-extension-list');
     },

     /**
      * Synthesizes and initializes an HTML element for the extension metadata
      * given in |extension|.
      * @param {Object} extension A dictionary of extension metadata.
      * @private
      */
     createNode_: function(extension) {
      var template = $('template-collection').querySelector(
          '.extension-list-item-wrapper');
      var node = template.cloneNode(true);
      node.id = extension.id;

      if (!extension.enabled)
        node.classList.add('disabled-extension');

      if (!extension.mayDisable)
        node.classList.add('may-not-disable');

      var item = node.querySelector('.extension-list-item');
      item.style.backgroundImage = 'url(' + extension.icon + ')';

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      var version = node.querySelector('.extension-version');
      version.textContent = extension.version;

      var description = node.querySelector('.extension-description');
      description.textContent = extension.description;

      // The 'Show Browser Action' button.
      if (extension.enable_show_button) {
        var showButton = node.querySelector('.show-button');
        showButton.addEventListener('click', function(e) {
          chrome.send('extensionSettingsShowButton', [extension.id]);
        });
        showButton.hidden = false;
      }

      // The 'allow in incognito' checkbox.
      var incognito = node.querySelector('.incognito-control');
      var butterBar = node.querySelector('.butter-bar');
      incognito.addEventListener('click', function(e) {
        var checked = e.target.checked;
        butterBarVisibility[extension.id] = checked;
        butterBar.hidden = !checked || extension.is_hosted_app;
        chrome.send('extensionSettingsEnableIncognito',
                    [extension.id, String(checked)]);
      });
      incognito.querySelector('input').checked = extension.enabledIncognito;
      butterBar.hidden = !butterBarVisibility[extension.id];

      // The 'allow file:// access' checkbox.
      if (extension.wantsFileAccess) {
        var fileAccess = node.querySelector('.file-access-control');
        fileAccess.addEventListener('click', function(e) {
          chrome.send('extensionSettingsAllowFileAccess',
                      [extension.id, String(e.target.checked)]);
        });
        fileAccess.querySelector('input').checked = extension.allowFileAccess;
        fileAccess.hidden = false;
      }

      // The 'Options' checkbox.
      if (extension.enabled && extension.optionsUrl) {
        var options = node.querySelector('.options-link');
        options.addEventListener('click', function(e) {
          chrome.send('extensionSettingsOptions', [extension.id]);
          e.preventDefault();
        });
        options.hidden = false;
      }

      if (extension.allow_activity) {
        var activity = node.querySelector('.activity-link');
        activity.href = 'chrome://extension-activity?extensionId=' +
            extension.id;
        activity.hidden = false;
      }

      // The 'View in Web Store/View Web Site' link.
      if (extension.homepageUrl) {
        var siteLink = node.querySelector('.site-link');
        siteLink.href = extension.homepageUrl;
        siteLink.textContent = loadTimeData.getString(
                extension.homepageProvided ? 'extensionSettingsVisitWebsite' :
                                             'extensionSettingsVisitWebStore');
        siteLink.hidden = false;
      }

      // The 'Reload' checkbox.
      if (extension.allow_reload) {
        var reload = node.querySelector('.reload-link');
        reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
        reload.hidden = false;
      }

      if (!extension.terminated) {
        // The 'Enabled' checkbox.
        var enable = node.querySelector('.enable-checkbox');
        enable.hidden = false;
        enable.querySelector('input').disabled = !extension.mayDisable;

        if (extension.mayDisable) {
          enable.addEventListener('click', function(e) {
            chrome.send('extensionSettingsEnable',
                        [extension.id, e.target.checked ? 'true' : 'false']);
          });
        }

        enable.querySelector('input').checked = extension.enabled;
      } else {
        var terminated_reload = node.querySelector('.terminated-reload-link');
        terminated_reload.hidden = false;
        terminated_reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
      }

      // 'Remove' button.
      var trashTemplate = $('template-collection').querySelector('.trash');
      var trash = trashTemplate.cloneNode(true);
      trash.title = loadTimeData.getString('extensionUninstall');
      trash.addEventListener('click', function(e) {
        chrome.send('extensionSettingsUninstall', [extension.id]);
      });
      node.querySelector('.enable-controls').appendChild(trash);

      // Developer mode ////////////////////////////////////////////////////////

      // First we have the id.
      var idLabel = node.querySelector('.extension-id');
      idLabel.textContent = ' ' + extension.id;

      // Then the path, if provided by unpacked extension.
      if (extension.isUnpacked) {
        var loadPath = node.querySelector('.load-path');
        loadPath.hidden = false;
        loadPath.querySelector('span:nth-of-type(2)').textContent =
            ' ' + extension.path;
      }

      // Then the 'managed, cannot uninstall/disable' message.
      if (!extension.mayDisable)
        node.querySelector('.managed-message').hidden = false;

      // Then active views.
      if (extension.views.length > 0) {
        var activeViews = node.querySelector('.active-views');
        activeViews.hidden = false;
        var link = activeViews.querySelector('a');

        extension.views.forEach(function(view, i) {
          var label = view.path +
              (view.incognito ?
                  ' ' + loadTimeData.getString('viewIncognito') : '') +
              (view.renderProcessId == -1 ?
                  ' ' + loadTimeData.getString('viewInactive') : '');
          link.textContent = label;
          link.addEventListener('click', function(e) {
            // TODO(estade): remove conversion to string?
            chrome.send('extensionSettingsInspect', [
              String(extension.id),
              String(view.renderProcessId),
              String(view.renderViewId),
              view.incognito
            ]);
          });

          if (i < extension.views.length - 1) {
            link = link.cloneNode(true);
            activeViews.appendChild(link);
          }
        });
      }

      // The install warnings.
      if (extension.installWarnings) {
        var panel = node.querySelector('.install-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.installWarnings.forEach(function(warning) {
          list.appendChild(document.createElement('li')).innerText = warning;
        });
      }

      this.appendChild(node);
    },
  };

  return {
    ExtensionsList: ExtensionsList
  };
});
