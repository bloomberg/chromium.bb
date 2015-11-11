// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * Compares two extensions to determine which should come first in the list.
   * @param {chrome.developerPrivate.ExtensionInfo} a
   * @param {chrome.developerPrivate.ExtensionInfo} b
   * @return {number}
   */
  var compareExtensions = function(a, b) {
    function compare(x, y) {
      return x < y ? -1 : (x > y ? 1 : 0);
    }
    function compareLocation(x, y) {
      if (x.location == y.location)
        return 0;
      if (x.location == chrome.developerPrivate.Location.UNPACKED)
        return -1;
      if (y.location == chrome.developerPrivate.Location.UNPACKED)
        return 1;
      return 0;
    }
    return compareLocation(a, b) ||
           compare(a.name.toLowerCase(), b.name.toLowerCase()) ||
           compare(a.id, b.id);
  };

  /**
   * @constructor
   * @implements {extensions.ItemDelegate}
   * @implements {extensions.SidebarDelegate}
   */
  function Service() {}

  Service.prototype = {
    /** @private {boolean} */
    promptIsShowing_: false,

    /** @param {extensions.Manager} manager */
    managerReady: function(manager) {
      this.manager_ = manager;
      this.sidebar_ = manager.sidebar;
      this.sidebar_.setDelegate(this);
      chrome.developerPrivate.onProfileStateChanged.addListener(
          this.onProfileStateChanged_.bind(this));
      chrome.developerPrivate.onItemStateChanged.addListener(
          this.onItemStateChanged_.bind(this));
      chrome.developerPrivate.getExtensionsInfo(
          {includeDisabled: true, includeTerminated: true},
          function(extensions) {
        extensions.sort(compareExtensions);
        this.extensions_ = extensions;
        for (let extension of extensions)
          this.manager_.addItem(extension, this);
      }.bind(this));
      chrome.developerPrivate.getProfileConfiguration(
          this.onProfileStateChanged_.bind(this));
    },

    /**
     * @param {chrome.developerPrivate.ProfileInfo} profileInfo
     * @private
     */
    onProfileStateChanged_: function(profileInfo) {
      this.profileInfo_ = profileInfo;
      this.sidebar_.inDevMode = profileInfo.inDeveloperMode;
      this.manager_.forEachItem(function(item) {
        item.inDevMode = profileInfo.inDeveloperMode;
      });
    },

    /**
     * @param {chrome.developerPrivate.EventData} eventData
     * @private
     */
    onItemStateChanged_: function(eventData) {
      var EventType = chrome.developerPrivate.EventType;
      switch (eventData.event_type) {
        case EventType.VIEW_REGISTERED:
        case EventType.VIEW_UNREGISTERED:
        case EventType.INSTALLED:
        case EventType.LOADED:
        case EventType.UNLOADED:
        case EventType.ERROR_ADDED:
        case EventType.ERRORS_REMOVED:
        case EventType.PREFS_CHANGED:
          // |extensionInfo| can be undefined in the case of an extension
          // being unloaded right before uninstallation. There's nothing to do
          // here.
          if (!eventData.extensionInfo)
            break;

          var existing = this.manager_.getItem(eventData.extensionInfo.id);
          if (existing) {
            existing.data = eventData.extensionInfo;
          } else {
            // If there's no existing item in the list, there shouldn't be an
            // extension with the same id in the data.
            var currentIndex = this.extensions_.findIndex(function(extension) {
              return extension.id == eventData.extensionInfo.id;
            });
            assert(currentIndex == -1);
            var newIndex = this.extensions_.findIndex(function(extension) {
              return compareExtensions(extension,
                                       assert(eventData.extensionInfo)) > 0;
            });
            newIndex = newIndex == -1 ? this.extensions_.length : newIndex;
            this.extensions_.splice(newIndex, 0, eventData.extensionInfo);
            this.manager_.addItem(eventData.extensionInfo, this, newIndex);
          }
          break;
        case EventType.UNINSTALLED:
          var currentIndex = this.extensions_.findIndex(function(extension) {
            return extension.id == eventData.item_id;
          });
          this.extensions_.splice(currentIndex, 1);
          this.manager_.removeItem(eventData.item_id);
          break;
        default:
          assertNotReached();
      }
    },

    /** @override */
    deleteItem: function(id) {
      if (this.promptIsShowing_)
        return;
      this.promptIsShowing_ = true;
      chrome.management.uninstall(id, {showConfirmDialog: true}, function() {
        // The "last error" was almost certainly the user canceling the dialog.
        // Do nothing. We only check it so we don't get noisy logs.
        /** @suppress {suspiciousCode} */
        chrome.runtime.lastError;
        this.promptIsShowing_ = false;
      }.bind(this));
    },

    /** @override */
    setItemEnabled: function(id, isEnabled) {
      chrome.management.setEnabled(id, isEnabled);
    },

    /** @override */
    showItemDetails: function(id) {},

    /** @override */
    setItemAllowedIncognito: function(id, isAllowedIncognito) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        incognitoAccess: isAllowedIncognito,
      });
    },

    /** @override */
    isInDevMode: function() {
      // It's possible this could be called before the profileInfo is
      // initialized; that's fine, because we'll update the items when it is.
      return !!this.profileInfo_ && this.profileInfo_.inDeveloperMode;
    },

    /** @override */
    inspectItemView: function(id, view) {
      chrome.developerPrivate.openDevTools({
        extensionId: id,
        renderProcessId: view.renderProcessId,
        renderViewId: view.renderViewId,
        incognito: view.incognito,
      });
    },

    /** @override */
    setProfileInDevMode: function(inDevMode) {
      chrome.developerPrivate.updateProfileConfiguration(
          {inDeveloperMode: inDevMode});
    },

    /** @override */
    loadUnpacked: function() {
      chrome.developerPrivate.loadUnpacked({failQuietly: true});
    },

    /** @override */
    packExtension: function() {
    },

    /** @override */
    updateAllExtensions: function() {
      chrome.developerPrivate.autoUpdate();
    },
  };

  cr.addSingletonGetter(Service);

  return {Service: Service};
});
