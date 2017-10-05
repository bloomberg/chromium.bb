// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /**
   * @implements {extensions.ErrorPageDelegate}
   * @implements {extensions.ItemDelegate}
   * @implements {extensions.LoadErrorDelegate}
   * @implements {extensions.PackDialogDelegate}
   * @implements {extensions.ToolbarDelegate}
   */
  class Service {
    constructor() {
      /** @private {boolean} */
      this.isDeleting_ = false;

      /** @private {extensions.Manager} */
      this.manager_;

      /** @private {Array<chrome.developerPrivate.ExtensionInfo>} */
      this.extensions_;
    }

    /** @param {extensions.Manager} manager */
    managerReady(manager) {
      this.manager_ = manager;
      this.manager_.set('delegate', this);

      const keyboardShortcuts = this.manager_.keyboardShortcuts;
      keyboardShortcuts.addEventListener(
          'shortcut-updated', this.onExtensionCommandUpdated_.bind(this));
      keyboardShortcuts.addEventListener(
          'shortcut-capture-started',
          this.onShortcutCaptureChanged_.bind(this, true));
      keyboardShortcuts.addEventListener(
          'shortcut-capture-ended',
          this.onShortcutCaptureChanged_.bind(this, false));
      chrome.developerPrivate.onProfileStateChanged.addListener(
          this.onProfileStateChanged_.bind(this));
      chrome.developerPrivate.onItemStateChanged.addListener(
          this.onItemStateChanged_.bind(this));
      chrome.developerPrivate.getExtensionsInfo(
          {includeDisabled: true, includeTerminated: true}, extensions => {
            this.extensions_ = extensions;
            for (let extension of extensions)
              this.manager_.addItem(extension);

            this.manager_.initPage();
          });
      chrome.developerPrivate.getProfileConfiguration(
          this.onProfileStateChanged_.bind(this));
    }

    /**
     * @param {chrome.developerPrivate.ProfileInfo} profileInfo
     * @private
     */
    onProfileStateChanged_(profileInfo) {
      this.manager_.set('inDevMode', profileInfo.inDeveloperMode);
    }

    /**
     * @param {chrome.developerPrivate.EventData} eventData
     * @private
     */
    onItemStateChanged_(eventData) {
      const currentIndex = this.extensions_.findIndex(function(extension) {
        return extension.id == eventData.item_id;
      });

      const EventType = chrome.developerPrivate.EventType;
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

          if (currentIndex >= 0) {
            this.extensions_[currentIndex] = eventData.extensionInfo;
            this.manager_.updateItem(eventData.extensionInfo);
          } else {
            this.extensions_.push(eventData.extensionInfo);
            this.manager_.addItem(eventData.extensionInfo);
          }
          break;
        case EventType.UNINSTALLED:
          this.manager_.removeItem(this.extensions_[currentIndex]);
          this.extensions_.splice(currentIndex, 1);
          break;
        default:
          assertNotReached();
      }
    }

    /**
     * Opens a file browser dialog for the user to select a file (or directory).
     * @param {chrome.developerPrivate.SelectType} selectType
     * @param {chrome.developerPrivate.FileType} fileType
     * @return {Promise<string>} The promise to be resolved with the selected
     *     path.
     */
    chooseFilePath_(selectType, fileType) {
      return new Promise(function(resolve, reject) {
        chrome.developerPrivate.choosePath(
            selectType, fileType, function(path) {
              if (chrome.runtime.lastError &&
                  chrome.runtime.lastError != 'File selection was canceled.') {
                reject(chrome.runtime.lastError);
              } else {
                resolve(path || '');
              }
            });
      });
    }

    /**
     * Updates an extension command.
     * @param {!CustomEvent} e
     * @private
     */
    onExtensionCommandUpdated_(e) {
      chrome.developerPrivate.updateExtensionCommand({
        extensionId: e.detail.item,
        commandName: e.detail.commandName,
        keybinding: e.detail.keybinding,
      });
    }

    /**
     * Called when shortcut capturing changes in order to suspend or re-enable
     * global shortcut handling. This is important so that the shortcuts aren't
     * processed normally as the user types them.
     * TODO(devlin): From very brief experimentation, it looks like preventing
     * the default handling on the event also does this. Investigate more in the
     * future.
     * @param {boolean} isCapturing
     * @param {!CustomEvent} e
     * @private
     */
    onShortcutCaptureChanged_(isCapturing, e) {
      chrome.developerPrivate.setShortcutHandlingSuspended(isCapturing);
    }

    /**
     * Attempts to load an unpacked extension, optionally as another attempt at
     * a previously-specified load.
     * @param {string=} opt_retryGuid
     * @private
     */
    loadUnpackedHelper_(opt_retryGuid) {
      chrome.developerPrivate.loadUnpacked(
          {failQuietly: true, populateError: true, retryGuid: opt_retryGuid},
          (loadError) => {
            if (chrome.runtime.lastError &&
                chrome.runtime.lastError.message !=
                    'File selection was canceled.') {
              throw new Error(chrome.runtime.lastError.message);
            }
            if (loadError) {
              this.manager_.loadError.loadError = loadError;
              this.manager_.loadError.show();
            }
          });
    }

    /** @override */
    deleteItem(id) {
      if (this.isDeleting_)
        return;
      this.isDeleting_ = true;
      chrome.management.uninstall(id, {showConfirmDialog: true}, () => {
        // The "last error" was almost certainly the user canceling the dialog.
        // Do nothing. We only check it so we don't get noisy logs.
        /** @suppress {suspiciousCode} */
        chrome.runtime.lastError;
        this.isDeleting_ = false;
      });
    }

    /** @override */
    setItemEnabled(id, isEnabled) {
      chrome.management.setEnabled(id, isEnabled);
    }

    /** @override */
    setItemAllowedIncognito(id, isAllowedIncognito) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        incognitoAccess: isAllowedIncognito,
      });
    }

    /** @override */
    setItemAllowedOnFileUrls(id, isAllowedOnFileUrls) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        fileAccess: isAllowedOnFileUrls,
      });
    }

    /** @override */
    setItemAllowedOnAllSites(id, isAllowedOnAllSites) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        runOnAllUrls: isAllowedOnAllSites,
      });
    }

    /** @override */
    setItemCollectsErrors(id, collectsErrors) {
      chrome.developerPrivate.updateExtensionConfiguration({
        extensionId: id,
        errorCollection: collectsErrors,
      });
    }

    /** @override */
    inspectItemView(id, view) {
      chrome.developerPrivate.openDevTools({
        extensionId: id,
        renderProcessId: view.renderProcessId,
        renderViewId: view.renderViewId,
        incognito: view.incognito,
      });
    }

    /** @override */
    reloadItem(id) {
      chrome.developerPrivate.reload(id, {failQuietly: false});
    }

    /** @override */
    repairItem(id) {
      chrome.developerPrivate.repairExtension(id);
    }

    /** @override */
    showItemOptionsPage(id) {
      const extension = this.extensions_.find(function(e) {
        return e.id == id;
      });
      assert(extension && extension.optionsPage);
      if (extension.optionsPage.openInTab) {
        chrome.developerPrivate.showOptions(id);
      } else {
        extensions.navigation.navigateTo(
            {page: Page.DETAILS, subpage: Dialog.OPTIONS, extensionId: id});
      }
    }

    /** @override */
    setProfileInDevMode(inDevMode) {
      chrome.developerPrivate.updateProfileConfiguration(
          {inDeveloperMode: inDevMode});
    }

    /** @override */
    loadUnpacked() {
      this.loadUnpackedHelper_();
    }

    /** @override */
    retryLoadUnpacked(retryGuid) {
      this.loadUnpackedHelper_(retryGuid);
    }

    /** @override */
    choosePackRootDirectory() {
      return this.chooseFilePath_(
          chrome.developerPrivate.SelectType.FOLDER,
          chrome.developerPrivate.FileType.LOAD);
    }

    /** @override */
    choosePrivateKeyPath() {
      return this.chooseFilePath_(
          chrome.developerPrivate.SelectType.FILE,
          chrome.developerPrivate.FileType.PEM);
    }

    /** @override */
    packExtension(rootPath, keyPath, flag, callback) {
      chrome.developerPrivate.packDirectory(rootPath, keyPath, flag, callback);
    }

    /** @override */
    updateAllExtensions() {
      chrome.developerPrivate.autoUpdate();
    }

    /** @override */
    deleteErrors(extensionId, errorIds, type) {
      chrome.developerPrivate.deleteExtensionErrors({
        extensionId: extensionId,
        errorIds: errorIds,
        type: type,
      });
    }

    /** @override */
    requestFileSource(args) {
      return new Promise(function(resolve, reject) {
        chrome.developerPrivate.requestFileSource(args, function(code) {
          resolve(code);
        });
      });
    }

    /** @override */
    openDevTools(args) {
      chrome.developerPrivate.openDevTools(args);
    }
  }

  cr.addSingletonGetter(Service);

  return {Service: Service};
});
