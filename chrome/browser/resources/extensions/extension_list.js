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

    /** @override */
    decorate: function() {
      this.textContent = '';

      this.showExtensionNodes_();
    },

    getIdQueryParam_: function() {
      return parseQueryParams(document.location)['id'];
    },

    /**
     * Creates all extension items from scratch.
     * @private
     */
    showExtensionNodes_: function() {
      // Iterate over the extension data and add each item to the list.
      this.data_.extensions.forEach(this.createNode_, this);

      var id_to_highlight = this.getIdQueryParam_();
      if (id_to_highlight) {
        // Scroll offset should be calculated slightly higher than the actual
        // offset of the element being scrolled to, so that it ends up not all
        // the way at the top. That way it is clear that there are more elements
        // above the element being scrolled to.
        var scroll_fudge = 1.2;
        var offset = $(id_to_highlight).offsetTop -
                     (scroll_fudge * $(id_to_highlight).clientHeight);
        var wrapper = this.parentNode;
        var list = wrapper.parentNode;
        list.scrollTop = offset;
      }

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

      if (!extension.enabled || extension.terminated)
        node.classList.add('inactive-extension');

      if (!extension.userModifiable)
        node.classList.add('may-not-disable');

      var id_to_highlight = this.getIdQueryParam_();
      if (node.id == id_to_highlight)
        node.classList.add('extension-highlight');

      var item = node.querySelector('.extension-list-item');
      item.style.backgroundImage = 'url(' + extension.icon + ')';

      var title = node.querySelector('.extension-title');
      title.textContent = extension.name;

      var version = node.querySelector('.extension-version');
      version.textContent = extension.version;

      var disableReason = node.querySelector('.extension-disable-reason');
      disableReason.textContent = extension.disableReason;

      var locationText = node.querySelector('.location-text');
      locationText.textContent = extension.locationText;

      var description = node.querySelector('.extension-description span');
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
      var incognito = node.querySelector('.incognito-control input');
      incognito.disabled = !extension.incognitoCanBeEnabled;
      incognito.checked = extension.enabledIncognito;
      if (!incognito.disabled) {
        incognito.addEventListener('change', function(e) {
          var checked = e.target.checked;
          butterBarVisibility[extension.id] = checked;
          butterBar.hidden = !checked || extension.is_hosted_app;
          chrome.send('extensionSettingsEnableIncognito',
                      [extension.id, String(checked)]);
        });
      }
      var butterBar = node.querySelector('.butter-bar');
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

      // The 'Options' link.
      if (extension.enabled && extension.optionsUrl) {
        var options = node.querySelector('.options-link');
        options.addEventListener('click', function(e) {
          chrome.send('extensionSettingsOptions', [extension.id]);
          e.preventDefault();
        });
        options.hidden = false;
      }

      // The 'Permissions' link.
      var permissions = node.querySelector('.permissions-link');
      permissions.addEventListener('click', function(e) {
        chrome.send('extensionSettingsPermissions', [extension.id]);
        e.preventDefault();
      });

      if (extension.allow_activity) {
        var activity = node.querySelector('.activity-link');
        activity.addEventListener('click', function(e) {
          chrome.send('navigateToUrl', [
            'chrome://extension-activity?extensionId=' + extension.id,
            '_blank',
            e.button,
            e.altKey,
            e.ctrlKey,
            e.metaKey,
            e.shiftKey
          ]);
          e.preventDefault();
        });
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

      if (extension.allow_reload) {
        // The 'Reload' link.
        var reload = node.querySelector('.reload-link');
        reload.addEventListener('click', function(e) {
          chrome.send('extensionSettingsReload', [extension.id]);
        });
        reload.hidden = false;

        if (extension.is_platform_app) {
          // The 'Launch' link.
          var launch = node.querySelector('.launch-link');
          launch.addEventListener('click', function(e) {
            chrome.send('extensionSettingsLaunch', [extension.id]);
          });
          launch.hidden = false;

          // The 'Restart' link.
          var restart = node.querySelector('.restart-link');
          restart.addEventListener('click', function(e) {
            chrome.send('extensionSettingsRestart', [extension.id]);
          });
          restart.hidden = false;
        }
      }

      if (!extension.terminated) {
        // The 'Enabled' checkbox.
        var enable = node.querySelector('.enable-checkbox');
        enable.hidden = false;
        enable.querySelector('input').disabled = !extension.userModifiable;

        if (extension.userModifiable) {
          enable.addEventListener('click', function(e) {
            chrome.send('extensionSettingsEnable',
                        [extension.id, e.target.checked ? 'true' : 'false']);

            // This may seem counter-intuitive (to not set/clear the checkmark)
            // but this page will be updated asynchronously if the extension
            // becomes enabled/disabled. It also might not become enabled or
            // disabled, because the user might e.g. get prompted when enabling
            // and choose not to.
            e.preventDefault();
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
      if (!extension.userModifiable)
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

      // The extension warnings (describing runtime issues).
      if (extension.warnings) {
        var panel = node.querySelector('.extension-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.warnings.forEach(function(warning) {
          list.appendChild(document.createElement('li')).innerText = warning;
        });
      }

      // The install warnings.
      if (extension.installWarnings) {
        var panel = node.querySelector('.install-warnings');
        panel.hidden = false;
        var list = panel.querySelector('ul');
        extension.installWarnings.forEach(function(warning) {
          var li = document.createElement('li');
          li[warning.isHTML ? 'innerHTML' : 'innerText'] = warning.message;
          list.appendChild(li);
        });
      }

      this.appendChild(node);
    }
  };

  return {
    ExtensionsList: ExtensionsList
  };
});
