// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/800945): Rename it to WallpaperPicker for consistency.

/**
 * WallpaperManager constructor.
 *
 * WallpaperManager objects encapsulate the functionality of the wallpaper
 * manager extension.
 *
 * @constructor
 * @param {HTMLElement} dialogDom The DOM node containing the prototypical
 *     extension UI.
 */

function WallpaperManager(dialogDom) {
  this.dialogDom_ = dialogDom;
  this.document_ = dialogDom.ownerDocument;
  this.useNewWallpaperPicker_ =
      loadTimeData.getBoolean('useNewWallpaperPicker');
  this.enableOnlineWallpaper_ = loadTimeData.valueExists('manifestBaseURL') ||
      this.useNewWallpaperPicker_;
  this.selectedItem_ = null;
  this.progressManager_ = new ProgressManager();
  this.customWallpaperData_ = null;
  this.currentWallpaper_ = null;
  this.wallpaperRequest_ = null;
  this.wallpaperDirs_ = WallpaperDirectories.getInstance();
  this.preDownloadDomInit_();

  // Uses the redesigned wallpaper picker if |useNewWallpaperPicker| is true.
  //
  // The old wallpaper picker fetches the manifest file once, and parse the info
  // (such as image url) from the file every time when the images need to be
  // displayed.
  // The new wallpaper picker has two steps: it first fetches a list of
  // collection names (ie. categories such as Art, Landscape etc.) via extension
  // API, and then fetches the info specific to each collection and caches the
  // info in a map.
  // After the url and relevant info of the images are fetched, the two share
  // the same path, ie. pass the info to |WallpaperThumbnailsGridItem| for
  // actual image rendering.
  this.document_.body.classList.toggle('v2', this.useNewWallpaperPicker_);
  this.placeWallpaperPicker_();

  if (this.useNewWallpaperPicker_) {
    // |collectionsInfo_| represents the list of wallpaper collections. Each
    // collection contains the display name and a unique id.
    this.collectionsInfo_ = null;
    // |imagesInfoMap_| caches the mapping between each collection id and the
    // images that belong to this collection. Each image is represented by a set
    // of info including the image url, author name, layout etc. Such info will
    // be used by |WallpaperThumbnailsGridItem| to display the images.
    this.imagesInfoMap_ = {};
    // The total count of images whose info has been fetched.
    this.imagesInfoCount_ = 0;
    this.getCollectionsInfo_();
  } else {
    this.fetchManifest_();
  }
}

// Anonymous 'namespace'.
// TODO(bshe): Get rid of anonymous namespace.
(function() {

/**
 * URL of the learn more page for wallpaper picker.
 *
 * @const
 */
var LearnMoreURL =
    'https://support.google.com/chromebook/?p=wallpaper_fileerror&hl=' +
    navigator.language;

/**
 * Index of the All category. It is the first category in wallpaper picker.
 *
 * @const
 */
var AllCategoryIndex = 0;

/**
 * Index offset of categories parsed from manifest. The All category is added
 * before them. So the offset is 1.
 *
 * @const
 */
var OnlineCategoriesOffset = 1;

/**
 * Returns a translated string.
 *
 * Wrapper function to make dealing with translated strings more concise.
 *
 * @param {string} id The id of the string to return.
 * @return {string} The translated string.
 */
function str(id) {
  return loadTimeData.getString(id);
}

/**
 * Returns the base name for |file_path|.
 * @param {string} file_path The path of the file.
 * @return {string} The base name of the file.
 */
function getBaseName(file_path) {
  return file_path.substring(file_path.lastIndexOf('/') + 1);
}

/**
 * Retruns the current selected layout.
 * @return {string} The selected layout.
 */
function getSelectedLayout() {
  var setWallpaperLayout = $('set-wallpaper-layout');
  return setWallpaperLayout.options[setWallpaperLayout.selectedIndex].value;
}

/**
 * Loads translated strings.
 */
WallpaperManager.initStrings = function(callback) {
  chrome.wallpaperPrivate.getStrings(function(strings) {
    loadTimeData.data = strings;
    if (callback)
      callback();
  });
};

/**
 * Requests wallpaper manifest file from server.
 */
WallpaperManager.prototype.fetchManifest_ = function() {
  var locale = navigator.language;
  if (!this.enableOnlineWallpaper_) {
    this.postDownloadDomInit_();
    return;
  }

  var urls = [
    str('manifestBaseURL') + locale + '.json',
    // Fallback url. Use 'en' locale by default.
    str('manifestBaseURL') + 'en.json'
  ];

  var asyncFetchManifestFromUrls = function(
      urls, func, successCallback, failureCallback) {
    var index = 0;
    var loop = {
      next: function() {
        if (index < urls.length) {
          func(loop, urls[index]);
          index++;
        } else {
          failureCallback();
        }
      },

      success: function(response) {
        successCallback(response);
      },

      failure: function() {
        failureCallback();
      }
    };
    loop.next();
  };

  var fetchManifestAsync = function(loop, url) {
    var xhr = new XMLHttpRequest();
    try {
      xhr.addEventListener('loadend', function(e) {
        if (this.status == 200 && this.responseText != null) {
          try {
            var manifest = JSON.parse(this.responseText);
            loop.success(manifest);
          } catch (e) {
            loop.failure();
          }
        } else {
          loop.next();
        }
      });
      xhr.open('GET', url, true);
      xhr.send(null);
    } catch (e) {
      loop.failure();
    }
  };

  if (navigator.onLine) {
    asyncFetchManifestFromUrls(
        urls, fetchManifestAsync, this.onLoadManifestSuccess_.bind(this),
        this.onLoadManifestFailed_.bind(this));
  } else {
    // If device is offline, fetches manifest from local storage.
    // TODO(bshe): Always loading the offline manifest first and replacing
    // with the online one when available.
    this.onLoadManifestFailed_();
  }
};

/**
 * Fetches wallpaper collection info.
 * @private
 */
WallpaperManager.prototype.getCollectionsInfo_ = function() {
  chrome.wallpaperPrivate.getCollectionsInfo(collectionsInfo => {
    if (chrome.runtime.lastError) {
      // TODO(crbug.com/800945): Distinguish the error types and show custom
      // error messages.
      this.showError_(str('connectionFailed'));
      $('wallpaper-grid').classList.add('image-picker-offline');
      this.postDownloadDomInit_();
      return;
    }

    this.collectionsInfo_ = collectionsInfo;
    var getIndividualCollectionInfo = index => {
      var collectionId = this.collectionsInfo_[index]['collectionId'];

      chrome.wallpaperPrivate.getImagesInfo(collectionId, imagesInfo => {
        var wallpapersDataModel = new cr.ui.ArrayDataModel([]);

        if (!chrome.runtime.lastError) {
          for (var i = 0; i < imagesInfo.length; ++i) {
            var wallpaperInfo = {
              // Use the next available unique id.
              wallpaperId: this.imagesInfoCount_,
              baseURL: imagesInfo[i]['imageUrl'],
              layout: Constants.WallpaperThumbnailDefaultLayout,
              source: Constants.WallpaperSourceEnum.Online,
              availableOffline: false,
              displayText: imagesInfo[i]['displayText'],
              authorWebsite: imagesInfo[i]['actionUrl'],
              collectionName: this.collectionsInfo_[index]['collectionName']
            };
            wallpapersDataModel.push(wallpaperInfo);
            ++this.imagesInfoCount_;
          }
        }
        // Save info to the map. The data model is empty if there's an error
        // fetching it.
        this.imagesInfoMap_[collectionId] = wallpapersDataModel;

        // If fetching info is completed for all the collections, start
        // initializing elements that depend on the fetched info (e.g. the
        // "current wallpaper" info bar), otherwise fetch the info for the next
        // collection.
        ++index;
        if (index >= this.collectionsInfo_.length)
          this.postDownloadDomInit_();
        else
          getIndividualCollectionInfo(index);
      });
    };

    getIndividualCollectionInfo(0 /*index=*/);
  });
};

/**
 * Displays images that belong to the particular collection on the new wallpaper
 * picker.
 * @param {number} index The index of the collection in |collectionsInfo_| list.
 * @private
 */
WallpaperManager.prototype.showCollection_ = function(index) {
  // Clear the 'no images' message if there's any.
  this.document_.body.classList.remove('no-images');
  var collectionId = this.collectionsInfo_[index]['collectionId'];
  if (!(collectionId in this.imagesInfoMap_)) {
    console.error('Attempt to display images with an unknown collection id.');
    return;
  }
  if (this.imagesInfoMap_[collectionId].length == 0) {
    // Show a 'no images' message to user.
    this.document_.body.classList.add('no-images');
    return;
  }
  this.wallpaperGrid_.dataModel = this.imagesInfoMap_[collectionId];
};

/**
 * Places the main dialog in the center of the screen, and the header bar at the
 * top. Only used for the new wallpaper picker.
 * @private
 */
WallpaperManager.prototype.placeWallpaperPicker_ = function() {
  if (!this.useNewWallpaperPicker_)
    return;
  var totalWidth = this.document_.body.offsetWidth;
  var totalHeight = this.document_.body.offsetHeight;
  $('top-header').style.left =
      (totalWidth - $('top-header').offsetWidth) / 2 + 'px';

  var setSuccessfullyMessage = $('set-successfully-message');
  setSuccessfullyMessage.style.left =
      (totalWidth - setSuccessfullyMessage.offsetWidth) / 2 + 'px';
  setSuccessfullyMessage.style.top =
      (totalWidth - setSuccessfullyMessage.offsetHeight) / 2 + 'px';

  var dialogContainer = this.document_.querySelector('.dialog-container');
  dialogContainer.style.left =
      (totalWidth - dialogContainer.offsetWidth) / 2 + 'px';
  dialogContainer.style.top =
      (totalHeight - dialogContainer.offsetHeight) / 2 + 'px';
};

/**
 * Shows error message in a centered dialog.
 * @private
 * @param {string} errroMessage The string to show in the error dialog.
 */
WallpaperManager.prototype.showError_ = function(errorMessage) {
  document.querySelector('.error-message').textContent = errorMessage;
  $('error-container').hidden = false;
};

/**
 * Sets manifest loaded from server. Called after manifest is successfully
 * loaded.
 * @param {object} manifest The parsed manifest file.
 */
WallpaperManager.prototype.onLoadManifestSuccess_ = function(manifest) {
  this.manifest_ = manifest;
  WallpaperUtil.saveToLocalStorage(Constants.AccessLocalManifestKey, manifest);
  this.postDownloadDomInit_();
};

// Sets manifest to previously saved object if any and shows connection error.
// Called after manifest failed to load.
WallpaperManager.prototype.onLoadManifestFailed_ = function() {
  var accessManifestKey = Constants.AccessLocalManifestKey;
  var self = this;
  Constants.WallpaperLocalStorage.get(accessManifestKey, function(items) {
    self.manifest_ = items[accessManifestKey] ? items[accessManifestKey] : null;
    self.showError_(str('connectionFailed'));
    self.postDownloadDomInit_();
    $('wallpaper-grid').classList.add('image-picker-offline');
  });
};

/**
 * Toggle surprise me feature of wallpaper picker. It fires an storage
 * onChanged event. Event handler for that event is in event_page.js.
 */
WallpaperManager.prototype.toggleSurpriseMe = function() {
  var shouldEnable = !WallpaperUtil.getSurpriseMeCheckboxValue();
  var onSuccess = () => {
    if (chrome.runtime.lastError == null) {
      if (shouldEnable) {
        // Hides the wallpaper set by message if there is any.
        $('wallpaper-set-by-message').textContent = '';
      } else if (this.wallpaperGrid_.activeItem) {
        // Unchecking the "Surprise me" checkbox falls back to the previous
        // wallpaper before "Surprise me" was turned on.
        this.setSelectedWallpaper_(this.wallpaperGrid_.activeItem);
        this.onWallpaperChanged_(
            this.wallpaperGrid_.activeItem, this.currentWallpaper_);
      }
      this.onSurpriseMeStateChanged_(shouldEnable);
    } else {
      // TODO(bshe): show error message to user.
      console.error('Failed to save surprise me option to chrome storage.');
    }
  };

  // To prevent the onChanged event being fired twice, we only save the value
  // to sync storage if the sync theme is enabled, otherwise save it to local
  // storage.
  WallpaperUtil.enabledSyncThemesCallback(syncEnabled => {
    if (syncEnabled)
      WallpaperUtil.saveToSyncStorage(
          Constants.AccessSyncSurpriseMeEnabledKey, shouldEnable, onSuccess);
    else
      WallpaperUtil.saveToLocalStorage(
          Constants.AccessLocalSurpriseMeEnabledKey, shouldEnable, onSuccess);
  });

  // Show a success message and quit after the user enables surprime me on
  // the new picker.
  if (shouldEnable && this.useNewWallpaperPicker_)
    this.showSuccessMessageAndQuit_();
};

/**
 * One-time initialization of various DOM nodes. Fetching manifest or the
 * collection info may take a long time due to slow connection. Dom nodes that
 * do not depend on the download should be initialized here.
 */
WallpaperManager.prototype.preDownloadDomInit_ = function() {
  $('window-close-button').addEventListener('click', function() {
    window.close();
  });
  this.document_.defaultView.addEventListener(
      'resize', this.onResize_.bind(this));
  this.document_.defaultView.addEventListener(
      'keydown', this.onKeyDown_.bind(this));
  $('learn-more').href = LearnMoreURL;
  $('close-error').addEventListener('click', function() {
    $('error-container').hidden = true;
  });
  $('close-wallpaper-selection').addEventListener('click', function() {
    $('wallpaper-selection-container').hidden = true;
    $('set-wallpaper-layout').disabled = true;
  });
};

/**
 * One-time initialization of various DOM nodes. Dom nodes that do depend on
 * the download should be initialized here.
 */
WallpaperManager.prototype.postDownloadDomInit_ = function() {
  i18nTemplate.process(this.document_, loadTimeData);
  this.initCategoriesList_();
  this.initThumbnailsGrid_();
  this.presetCategory_();

  $('file-selector')
      .addEventListener('change', this.onFileSelectorChanged_.bind(this));
  $('set-wallpaper-layout')
      .addEventListener('change', this.onWallpaperLayoutChanged_.bind(this));

  // On the new wallpaper picker, clicking the refresh button will force
  // fetching the online wallpapers from scratch.
  $('refresh').addEventListener('click', this.getCollectionsInfo_.bind(this));
  $('center').addEventListener(
      'click', this.setCustomWallpaperLayout_.bind(this, 'CENTER'));
  $('center-cropped')
      .addEventListener(
          'click', this.setCustomWallpaperLayout_.bind(this, 'CENTER_CROPPED'));

  // Always prefer the value from local filesystem to avoid the time window
  // of setting the third party app name and the third party wallpaper.
  var getThirdPartyAppName = function(callback) {
    Constants.WallpaperLocalStorage.get(
        Constants.AccessLocalWallpaperInfoKey, function(items) {
          var localInfo = items[Constants.AccessLocalWallpaperInfoKey];
          if (localInfo && localInfo.hasOwnProperty('appName'))
            callback(localInfo.appName);
          else
            callback('');
        });
  };

  getThirdPartyAppName(function(appName) {
    if (!!appName) {
      $('wallpaper-set-by-message').textContent =
          loadTimeData.getStringF('currentWallpaperSetByMessage', appName);
      $('wallpaper-grid').classList.add('small');
    } else {
      $('wallpaper-grid').classList.remove('small');
    }
  });

  if (this.enableOnlineWallpaper_) {
    this.document_.body.setAttribute('surprise-me-disabled', '');
    $('surprise-me').hidden = false;
    $('surprise-me')
        .addEventListener('click', this.toggleSurpriseMe.bind(this));

    WallpaperUtil.enabledSyncThemesCallback(syncEnabled => {
      // Surprise me has been moved from local to sync storage, prefer
      // values from sync, but if unset check local and update synced pref
      // if applicable.
      if (syncEnabled) {
        Constants.WallpaperSyncStorage.get(
            Constants.AccessSyncSurpriseMeEnabledKey, items => {
              if (items.hasOwnProperty(
                      Constants.AccessSyncSurpriseMeEnabledKey)) {
                if (items[Constants.AccessSyncSurpriseMeEnabledKey])
                  this.onSurpriseMeStateChanged_(true /*enabled=*/);
              } else {
                Constants.WallpaperLocalStorage.get(
                    Constants.AccessLocalSurpriseMeEnabledKey, items => {
                      if (items.hasOwnProperty(
                              Constants.AccessLocalSurpriseMeEnabledKey)) {
                        WallpaperUtil.saveToSyncStorage(
                            Constants.AccessSyncSurpriseMeEnabledKey,
                            items[Constants.AccessLocalSurpriseMeEnabledKey]);
                        if (items[Constants.AccessLocalSurpriseMeEnabledKey])
                          this.onSurpriseMeStateChanged_(true /*enabled=*/);
                      }
                    });
              }
            });
      } else {
        Constants.WallpaperLocalStorage.get(
            Constants.AccessLocalSurpriseMeEnabledKey, items => {
              if (items.hasOwnProperty(
                      Constants.AccessLocalSurpriseMeEnabledKey)) {
                if (items[Constants.AccessLocalSurpriseMeEnabledKey])
                  this.onSurpriseMeStateChanged_(true /*enabled=*/);
              }
            });
      }
    });

    window.addEventListener('offline', () => {
      chrome.wallpaperPrivate.getOfflineWallpaperList(lists => {
        if (!this.downloadedListMap_)
          this.downloadedListMap_ = {};
        for (var i = 0; i < lists.length; i++) {
          this.downloadedListMap_[lists[i]] = true;
        }
        var thumbnails = this.document_.querySelectorAll('.thumbnail');
        for (var i = 0; i < thumbnails.length; i++) {
          var thumbnail = thumbnails[i];
          var url = this.wallpaperGrid_.dataModel.item(i).baseURL;
          var fileName = getBaseName(url) + str('highResolutionSuffix');
          if (this.downloadedListMap_ &&
              this.downloadedListMap_.hasOwnProperty(encodeURI(fileName))) {
            thumbnail.offline = true;
          }
        }
      });
      $('wallpaper-grid').classList.add('image-picker-offline');
    });
    window.addEventListener('online', () => {
      this.downloadedListMap_ = null;
      $('wallpaper-grid').classList.remove('image-picker-offline');
    });
  }

  this.onResize_();
  this.initContextMenuAndCommand_();
  WallpaperUtil.testSendMessage('launched');
};

/**
 * One-time initialization of context menu and command.
 */
WallpaperManager.prototype.initContextMenuAndCommand_ = function() {
  this.wallpaperContextMenu_ = $('wallpaper-context-menu');
  cr.ui.Menu.decorate(this.wallpaperContextMenu_);
  cr.ui.contextMenuHandler.setContextMenu(
      this.wallpaperGrid_, this.wallpaperContextMenu_);
  var commands = this.dialogDom_.querySelectorAll('command');
  for (var i = 0; i < commands.length; i++)
    cr.ui.Command.decorate(commands[i]);

  var doc = this.document_;
  doc.addEventListener('command', this.onCommand_.bind(this));
  doc.addEventListener('canExecute', this.onCommandCanExecute_.bind(this));
};

/**
 * Handles a command being executed.
 * @param {Event} event A command event.
 */
WallpaperManager.prototype.onCommand_ = function(event) {
  if (event.command.id == 'delete') {
    var wallpaperGrid = this.wallpaperGrid_;
    var selectedIndex = wallpaperGrid.selectionModel.selectedIndex;
    var item = wallpaperGrid.dataModel.item(selectedIndex);
    if (!item || item.source != Constants.WallpaperSourceEnum.Custom)
      return;
    this.removeCustomWallpaper(item.baseURL);
    wallpaperGrid.dataModel.splice(selectedIndex, 1);
    // Calculate the number of remaining custom wallpapers. The add new button
    // in data model needs to be excluded.
    var customWallpaperCount = wallpaperGrid.dataModel.length - 1;
    if (customWallpaperCount == 0) {
      // Active custom wallpaper is also copied in chronos data dir. It needs
      // to be deleted.
      chrome.wallpaperPrivate.resetWallpaper();
      this.onWallpaperChanged_(null, null);
    } else {
      selectedIndex = Math.min(selectedIndex, customWallpaperCount - 1);
      wallpaperGrid.selectionModel.selectedIndex = selectedIndex;
    }
    event.cancelBubble = true;
  }
};

/**
 * Decides if a command can be executed on current target.
 * @param {Event} event A command event.
 */
WallpaperManager.prototype.onCommandCanExecute_ = function(event) {
  switch (event.command.id) {
    case 'delete':
      var wallpaperGrid = this.wallpaperGrid_;
      var selectedIndex = wallpaperGrid.selectionModel.selectedIndex;
      var item = wallpaperGrid.dataModel.item(selectedIndex);
      if (selectedIndex != this.wallpaperGrid_.dataModel.length - 1 && item &&
          item.source == Constants.WallpaperSourceEnum.Custom) {
        event.canExecute = true;
        break;
      }
    default:
      event.canExecute = false;
  }
};

/**
 * Preset to the category which contains current wallpaper.
 */
WallpaperManager.prototype.presetCategory_ = function() {
  // |currentWallpaper| is either a url containing |highResolutionSuffix| or a
  // custom wallpaper file name.
  this.currentWallpaper_ = str('currentWallpaper');
  if (this.useNewWallpaperPicker_) {
    // TODO(crbug.com/812725): Implement preset category for the new picker. For
    // now, autoselect the first category.
    this.categoriesList_.selectionModel.selectedIndex = 0;
    this.decorateCurrentWallpaperInfoBar_();
    return;
  }

  if (!this.enableOnlineWallpaper_ ||
      (this.currentWallpaper_ &&
       this.currentWallpaper_.indexOf(str('highResolutionSuffix')) == -1)) {
    // Custom is the last one in the categories list.
    this.categoriesList_.selectionModel.selectedIndex =
        this.categoriesList_.dataModel.length - 1;
    return;
  }
  var self = this;
  var presetCategoryInner = function() {
    // Selects the first category in the categories list of current
    // wallpaper as the default selected category when showing wallpaper
    // picker UI.
    var presetCategory = AllCategoryIndex;
    if (self.currentWallpaper_) {
      for (var key in self.manifest_.wallpaper_list) {
        var url = self.manifest_.wallpaper_list[key].base_url +
            str('highResolutionSuffix');
        if (url.indexOf(self.currentWallpaper_) != -1 &&
            self.manifest_.wallpaper_list[key].categories.length > 0) {
          presetCategory = self.manifest_.wallpaper_list[key].categories[0] +
              OnlineCategoriesOffset;
          break;
        }
      }
    }
    self.categoriesList_.selectionModel.selectedIndex = presetCategory;
  };
  if (navigator.onLine) {
    presetCategoryInner();
  } else {
    // If device is offline, gets the available offline wallpaper list first.
    // Wallpapers which are not in the list will display a grayscaled
    // thumbnail.
    chrome.wallpaperPrivate.getOfflineWallpaperList(function(lists) {
      if (!self.downloadedListMap_)
        self.downloadedListMap_ = {};
      for (var i = 0; i < lists.length; i++)
        self.downloadedListMap_[lists[i]] = true;
      presetCategoryInner();
    });
  }
};

/**
 * Decorate the info bar for current wallpaper which shows the image thumbnail,
 * title and description.
 * @private
 */
WallpaperManager.prototype.decorateCurrentWallpaperInfoBar_ = function() {
  // TODO(crbug.com/811619): Replace with i18n strings.
  $('currently-set-message').textContent = 'Currently set';
  var image = $('current-wallpaper-image');
  var currentWallpaperInfo;
  Object.values(this.imagesInfoMap_).forEach(imagesInfo => {
    for (var i = 0; i < imagesInfo.length; ++i) {
      if (this.currentWallpaper_.includes(imagesInfo.item(i).baseURL))
        currentWallpaperInfo = imagesInfo.item(i);
    }
  });
  if (currentWallpaperInfo) {
    WallpaperUtil.displayThumbnail(
        image, currentWallpaperInfo.baseURL,
        Constants.WallpaperSourceEnum.Online);
    $('current-wallpaper-title').textContent =
        currentWallpaperInfo.displayText[0];
    for (var i = 1; i < currentWallpaperInfo.displayText.length; ++i) {
      $('current-wallpaper-description')
          .appendChild(
              document.createTextNode(currentWallpaperInfo.displayText[i]));
    }
    $('current-wallpaper-explore-link').href =
        currentWallpaperInfo.authorWebsite;
    return;
  }

  // Show a placeholder as the image title if the current wallpaper is not found
  // in the list.
  $('current-wallpaper-title').textContent = str('customCategoryLabel');
  $('current-wallpaper-explore').hidden = true;
  var onSuccess = fileEntry => {
    image.src = fileEntry.toURL();
  };
  var onError = e => {
    console.error('Can not get thumbnail data for the current wallpaper.');
    // TODO(crbug.com/824453): Decide the right UI when this error happens.
  };
  wallpaperDirectories.getDirectory(
      Constants.WallpaperDirNameEnum.THUMBNAIL, dirEntry => {
        dirEntry.getFile(
            getBaseName(this.currentWallpaper_), {create: false}, onSuccess,
            onError);
      }, onError);
};

/**
 * Constructs the thumbnails grid.
 */
WallpaperManager.prototype.initThumbnailsGrid_ = function() {
  this.wallpaperGrid_ = $('wallpaper-grid');
  wallpapers.WallpaperThumbnailsGrid.decorate(this.wallpaperGrid_);

  this.wallpaperGrid_.addEventListener('change', this.onChange_.bind(this));
  if (!this.useNewWallpaperPicker_)
    this.wallpaperGrid_.addEventListener('dblclick', this.onClose_.bind(this));
};

/**
 * Handles change event dispatched by wallpaper grid.
 */
WallpaperManager.prototype.onChange_ = function() {
  // splice may dispatch a change event because the position of selected
  // element changing. But the actual selected element may not change after
  // splice. Check if the new selected element equals to the previous selected
  // element before continuing. Otherwise, wallpaper may reset to previous one
  // as described in http://crbug.com/229036.
  if (this.selectedItem_ == this.wallpaperGrid_.selectedItem)
    return;
  this.selectedItem_ = this.wallpaperGrid_.selectedItem;
  this.onSelectedItemChanged_();
};

/**
 * Closes window if no pending wallpaper request.
 */
WallpaperManager.prototype.onClose_ = function() {
  if (this.wallpaperRequest_) {
    this.wallpaperRequest_.addEventListener('loadend', function() {
      // Close window on wallpaper loading finished.
      window.close();
    });
  } else {
    window.close();
  }
};

/**
 * Moves the check mark to |activeItem| and hides the wallpaper set by third
 * party message if any. And saves the wallpaper's information to local & sync
 * storage. Called when wallpaper changed successfully.
 * @param {?Object} activeItem The active item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {?string} currentWallpaperURL The URL or filename of current
 *     wallpaper.
 */
WallpaperManager.prototype.onWallpaperChanged_ = function(
    activeItem, currentWallpaperURL) {
  this.wallpaperGrid_.activeItem = activeItem;
  this.currentWallpaper_ = currentWallpaperURL;
  // Hides the wallpaper set by message.
  $('wallpaper-set-by-message').textContent = '';
  $('wallpaper-grid').classList.remove('small');

  if (activeItem) {
    WallpaperUtil.saveWallpaperInfo(
        currentWallpaperURL, activeItem.layout, activeItem.source, '');
  } else {
    WallpaperUtil.saveWallpaperInfo(
        '', '', Constants.WallpaperSourceEnum.Default, '');
  }
};

/**
 * Sets wallpaper to the corresponding wallpaper of selected thumbnail.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @private
 */
WallpaperManager.prototype.setSelectedWallpaper_ = function(selectedItem) {
  switch (selectedItem.source) {
    case Constants.WallpaperSourceEnum.Custom:
      this.setSelectedCustomWallpaper_(
          selectedItem, (imageData, optThumbnailData) => {
            this.onWallpaperChanged_(selectedItem, selectedItem.baseURL);
            this.saveCustomWallpaperToSyncFS_(
                selectedItem.baseURL, getSelectedLayout(), imageData,
                optThumbnailData, this.onFileSystemError_.bind(this));
          });
      break;
    case Constants.WallpaperSourceEnum.OEM:
      // Resets back to default wallpaper.
      chrome.wallpaperPrivate.resetWallpaper();
      this.onWallpaperChanged_(selectedItem, selectedItem.baseURL);
      break;
    case Constants.WallpaperSourceEnum.Online:
      this.setSelectedOnlineWallpaper_(selectedItem);
      break;
    case Constants.WallpaperSourceEnum.Daily:
    case Constants.WallpaperSourceEnum.ThirdParty:
    default:
      console.error('Unsupported wallpaper source.');
  }
};

/**
 * Implementation of |setSelectedWallpaper_| for custom wallpapers.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {function} successCallback The success callback.
 * @private
 */
WallpaperManager.prototype.setSelectedCustomWallpaper_ = function(
    selectedItem, successCallback) {
  if (selectedItem.source != Constants.WallpaperSourceEnum.Custom) {
    console.error(
        '|setSelectedCustomWallpaper_| is called but the wallpaper source ' +
        'is not custom.');
    return;
  }

  if (this.useNewWallpaperPicker_)
    this.setCustomWallpaperSelectedOnNewPicker_(selectedItem, successCallback);
  else
    this.setCustomWallpaperSelectedOnOldPicker_(selectedItem, successCallback);
};

/**
 * Implementation of |setSelectedCustomWallpaper_| for the new wallpaper picker.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {function} successCallback The success callback.
 * @private
 */
WallpaperManager.prototype.setCustomWallpaperSelectedOnNewPicker_ = function(
    selectedItem, successCallback) {
  // Read the image data from |filePath| and set the wallpaper with the data.
  chrome.wallpaperPrivate.getLocalImageData(
      selectedItem.filePath, imageData => {
        var onPreviewCustomWallpaperFailure = function() {
          console.error('The attempt to preview custom wallpaper failed.');
          // TODO(crbug.com/810892): Show an error message to user.
        };

        if (chrome.runtime.lastError || !imageData) {
          onPreviewCustomWallpaperFailure();
          return;
        }

        chrome.wallpaperPrivate.setCustomWallpaper(
            imageData, selectedItem.layout, true /*generateThumbnail=*/,
            selectedItem.baseURL, true /*previewMode=*/, optThumbnailData => {
              if (chrome.runtime.lastError) {
                onPreviewCustomWallpaperFailure();
              } else {
                this.onPreviewModeStarted_(
                    successCallback.bind(null, imageData, optThumbnailData));
              }
            });
      });
};

/**
 * TODO(crbug.com/787134): Delete the method after the old picker is deprecated.
 *
 * Implementation of |setSelectedCustomWallpaper_| for the old wallpaper picker.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 * @param {function} successCallback The success callback.
 * @private
 */
WallpaperManager.prototype.setCustomWallpaperSelectedOnOldPicker_ = function(
    selectedItem, successCallback) {
  var errorHandler = this.onFileSystemError_.bind(this);
  var success = dirEntry => {
    dirEntry.getFile(selectedItem.baseURL, {create: false}, fileEntry => {
      fileEntry.file(file => {
        var reader = new FileReader();
        reader.readAsArrayBuffer(file);
        reader.addEventListener('error', errorHandler);
        reader.addEventListener('load', e => {
          // The thumbnail already exists at this point. There's no need to
          // regenerate it.
          this.setCustomWallpaperSelectedOnOldPickerImpl_(
              e.target.result, selectedItem.layout,
              false /*generateThumbnail=*/, selectedItem.baseURL,
              successCallback.bind(null, e.target.result), errorHandler);
        });
      }, errorHandler);
    }, errorHandler);
  };
  this.wallpaperDirs_.getDirectory(
      Constants.WallpaperDirNameEnum.ORIGINAL, success, errorHandler);
};

/**
 * Implementation of |setSelectedWallpaper_| for online wallpapers.
 * @param {Object} selectedItem The selected item in WallpaperThumbnailsGrid's
 *     data model.
 */
WallpaperManager.prototype.setSelectedOnlineWallpaper_ = function(
    selectedItem) {
  if (selectedItem.source != Constants.WallpaperSourceEnum.Online) {
    console.error(
        '|setSelectedOnlineWallpaper_| is called but the wallpaper source is ' +
        'not online.');
    return;
  }

  // Cancel any ongoing wallpaper request, otherwise the wallpaper being set in
  // the end may not be the one that the user selected the last, because the
  // time needed to set each wallpaper may vary (e.g. some wallpapers already
  // exist in the local file system but others need to be fetched from server).
  if (this.useNewWallpaperPicker_ && this.wallpaperRequest_) {
    this.wallpaperRequest_.abort();
    this.wallpaperRequest_ = null;
  }

  var wallpaperUrl = selectedItem.baseURL + str('highResolutionSuffix');
  var selectedGridItem = this.wallpaperGrid_.getListItem(selectedItem);
  var previewMode = this.useNewWallpaperPicker_;

  chrome.wallpaperPrivate.setWallpaperIfExists(
      wallpaperUrl, selectedItem.layout, previewMode, exists => {
        var successCallback = () => {
          if (previewMode) {
            this.onPreviewModeStarted_(this.onWallpaperChanged_.bind(
                this, selectedItem, wallpaperUrl));
          } else {
            this.onWallpaperChanged_(selectedItem, wallpaperUrl);
          }
        };

        if (exists) {
          successCallback();
          return;
        }

        // Falls back to request wallpaper from server.
        this.wallpaperRequest_ = new XMLHttpRequest();
        this.progressManager_.reset(this.wallpaperRequest_, selectedGridItem);

        var onSuccess =
            xhr => {
              var image = xhr.response;
              chrome.wallpaperPrivate.setWallpaper(
                  image, selectedItem.layout, wallpaperUrl, previewMode, () => {
                    this.progressManager_.hideProgressBar(selectedGridItem);

                    if (chrome.runtime.lastError != undefined &&
                        chrome.runtime.lastError.message !=
                            str('canceledWallpaper')) {
                      this.showError_(chrome.runtime.lastError.message);
                    } else {
                      successCallback();
                    }
                  });
              this.wallpaperRequest_ = null;
            };
        var onFailure = status => {
          this.progressManager_.hideProgressBar(selectedGridItem);
          this.showError_(str('downloadFailed'));
          this.wallpaperRequest_ = null;
        };
        WallpaperUtil.fetchURL(
            wallpaperUrl, 'arraybuffer', onSuccess, onFailure,
            this.wallpaperRequest_);
      });
};

/**
 * Handles the UI changes when the preview mode is started.
 * @param {function} confirmPreviewWallpaperCallback The callback after preview
 *     wallpaper is set.
 * @private
 */
WallpaperManager.prototype.onPreviewModeStarted_ = function(
    confirmPreviewWallpaperCallback) {
  this.document_.body.classList.add('preview-mode');
  // TODO(crbug.com/811619): Replace with i18n strings.
  $('confirm-preview-wallpaper').textContent = 'Set Wallpaper';
  $('cancel-preview-wallpaper').textContent = 'Cancel';

  var onConfirmClicked = () => {
    chrome.wallpaperPrivate.confirmPreviewWallpaper(() => {
      confirmPreviewWallpaperCallback();
      this.showSuccessMessageAndQuit_();
    });
  };
  $('confirm-preview-wallpaper').addEventListener('click', onConfirmClicked);

  var onCancelClicked = () => {
    $('confirm-preview-wallpaper')
        .removeEventListener('click', onConfirmClicked);
    $('cancel-preview-wallpaper').removeEventListener('click', onCancelClicked);
    chrome.wallpaperPrivate.cancelPreviewWallpaper(() => {
      // Deselect the image.
      this.wallpaperGrid_.selectedItem = null;
      this.document_.body.classList.remove('preview-mode');
    });
  };
  $('cancel-preview-wallpaper').addEventListener('click', onCancelClicked);
};

/*
 * Removes the oldest custom wallpaper. If the oldest one is set as current
 * wallpaper, removes the second oldest one to free some space. This should
 * only be called when exceeding wallpaper quota.
 */
WallpaperManager.prototype.removeOldestWallpaper_ = function() {
  // Custom wallpapers should already sorted when put to the data model. The
  // last element is the add new button, need to exclude it as well.
  var oldestIndex = this.wallpaperGrid_.dataModel.length - 2;
  var item = this.wallpaperGrid_.dataModel.item(oldestIndex);
  if (!item || item.source != Constants.WallpaperSourceEnum.Custom)
    return;
  if (item.baseURL == this.currentWallpaper_)
    item = this.wallpaperGrid_.dataModel.item(--oldestIndex);
  if (item) {
    this.removeCustomWallpaper(item.baseURL);
    this.wallpaperGrid_.dataModel.splice(oldestIndex, 1);
  }
};

/*
 * Shows a success message and closes the window.
 * @private
 */
WallpaperManager.prototype.showSuccessMessageAndQuit_ = function() {
  // TODO(crbug.com/811619): Replace with i18n strings.
  $('set-successfully-message').textContent = 'Wallpaper set successfully';
  this.document_.body.classList.add('wallpaper-set-successfully');
  // Close the window after showing the success message.
  window.setTimeout(() => {
    window.close();
  }, 2000);
};

/*
 * Shows an error message to user and log the failed reason in console.
 */
WallpaperManager.prototype.onFileSystemError_ = function(e) {
  var msg = '';
  switch (e.code) {
    case FileError.QUOTA_EXCEEDED_ERR:
      msg = 'QUOTA_EXCEEDED_ERR';
      // Instead of simply remove oldest wallpaper, we should consider a
      // better way to handle this situation. See crbug.com/180890.
      this.removeOldestWallpaper_();
      break;
    case FileError.NOT_FOUND_ERR:
      msg = 'NOT_FOUND_ERR';
      break;
    case FileError.SECURITY_ERR:
      msg = 'SECURITY_ERR';
      break;
    case FileError.INVALID_MODIFICATION_ERR:
      msg = 'INVALID_MODIFICATION_ERR';
      break;
    case FileError.INVALID_STATE_ERR:
      msg = 'INVALID_STATE_ERR';
      break;
    default:
      msg = 'Unknown Error';
      break;
  }
  console.error('Error: ' + msg);
  this.showError_(str('accessFileFailure'));
};

/**
 * Handles changing of selectedItem in wallpaper manager.
 */
WallpaperManager.prototype.onSelectedItemChanged_ = function() {
  this.setWallpaperAttribution(this.selectedItem_);

  if (!this.selectedItem_ || this.selectedItem_.source == 'ADDNEW')
    return;

  if (this.selectedItem_.baseURL && !this.wallpaperGrid_.inProgramSelection) {
    if (this.selectedItem_.source == Constants.WallpaperSourceEnum.Custom) {
      var items = {};
      var key = this.selectedItem_.baseURL;
      var self = this;
      Constants.WallpaperLocalStorage.get(key, function(items) {
        self.selectedItem_.layout =
            items[key] ? items[key] : Constants.WallpaperThumbnailDefaultLayout;
        self.setSelectedWallpaper_(self.selectedItem_);
      });
    } else {
      this.setSelectedWallpaper_(this.selectedItem_);
    }
  }
};

/**
 * Set attributions of wallpaper with given URL. If URL is not valid, clear
 * the attributions.
 * @param {Object} selectedItem the selected item in WallpaperThumbnailsGrid's
 *     data model.
 */
WallpaperManager.prototype.setWallpaperAttribution = function(selectedItem) {
  if (this.useNewWallpaperPicker_) {
    // The first element in |displayText| is used as title.
    if (selectedItem.displayText)
      $('image-title').textContent = selectedItem.displayText[0];
    $('collection-name').textContent = selectedItem.collectionName;
    var topHeaderContents =
        this.document_.querySelector('.top-header-contents');
    topHeaderContents.classList.toggle(
        'online', selectedItem.source === Constants.WallpaperSourceEnum.Online);
    topHeaderContents.classList.toggle(
        'custom', selectedItem.source === Constants.WallpaperSourceEnum.Custom);
    return;
  }

  // Only online wallpapers have author and website attributes. All other type
  // of wallpapers should not show attributions.
  if (!selectedItem ||
      selectedItem.source != Constants.WallpaperSourceEnum.Online) {
    $('wallpaper-attribute').hidden = true;
    $('attribute-image').hidden = true;
    $('author-name').textContent = '';
    $('author-website').textContent = $('author-website').href = '';
    $('attribute-image').src = '';
    return;
  }

  $('author-name').textContent = selectedItem.author;
  $('author-website').textContent = $('author-website').href =
      selectedItem.authorWebsite;
  var img = $('attribute-image');
  WallpaperUtil.displayThumbnail(
      img, selectedItem.baseURL, selectedItem.source);
  img.hidden = false;
  $('wallpaper-attribute').hidden = false;
};

/**
 * Resize thumbnails grid and categories list to fit the new window size.
 */
WallpaperManager.prototype.onResize_ = function() {
  this.placeWallpaperPicker_();
  this.wallpaperGrid_.redraw();
  this.categoriesList_.redraw();
};

/**
 * Close the last opened overlay or app window on pressing the Escape key.
 * @param {Event} event A keydown event.
 */
WallpaperManager.prototype.onKeyDown_ = function(event) {
  if (event.keyCode == 27) {
    // The last opened overlay coincides with the first match of querySelector
    // because the Error Container is declared in the DOM before the Wallpaper
    // Selection Container.
    // TODO(bshe): Make the overlay selection not dependent on the DOM.
    var closeButtonSelector = '.overlay-container:not([hidden]) .close';
    var closeButton = this.document_.querySelector(closeButtonSelector);
    if (closeButton) {
      closeButton.click();
      event.preventDefault();
    } else {
      this.onClose_();
    }
  }
};

/**
 * Constructs the categories list.
 */
WallpaperManager.prototype.initCategoriesList_ = function() {
  this.categoriesList_ = $('categories-list');
  wallpapers.WallpaperCategoriesList.decorate(this.categoriesList_);

  this.categoriesList_.selectionModel.addEventListener(
      'change', this.onCategoriesChange_.bind(this));

  if (this.useNewWallpaperPicker_ && this.collectionsInfo_) {
    for (var colletionInfo of this.collectionsInfo_)
      this.categoriesList_.dataModel.push(colletionInfo['collectionName']);
  } else if (this.enableOnlineWallpaper_ && this.manifest_) {
    // Adds all category as first category.
    this.categoriesList_.dataModel.push(str('allCategoryLabel'));
    for (var key in this.manifest_.categories) {
      this.categoriesList_.dataModel.push(this.manifest_.categories[key]);
    }
  }
  // Adds custom category as last category.
  this.categoriesList_.dataModel.push(str('customCategoryLabel'));
};

/**
 * Handles the custom wallpaper which user selected from file manager. Called
 * when users select a file.
 */
WallpaperManager.prototype.onFileSelectorChanged_ = function() {
  var files = $('file-selector').files;
  if (files.length != 1)
    console.error('More than one files are selected or no file selected');
  if (!files[0].type.match('image/jpeg') && !files[0].type.match('image/png')) {
    this.showError_(str('invalidWallpaper'));
    return;
  }
  var layout = getSelectedLayout();
  var self = this;
  var errorHandler = this.onFileSystemError_.bind(this);
  var setSelectedFile = function(file, layout, fileName) {
    var success = function(dirEntry) {
      dirEntry.getFile(fileName, {create: true}, function(fileEntry) {
        fileEntry.createWriter(function(fileWriter) {
          fileWriter.addEventListener('writeend', function(e) {
            var reader = new FileReader();
            reader.readAsArrayBuffer(file);
            reader.addEventListener('error', errorHandler);
            reader.addEventListener('load', function(e) {
              self.setCustomWallpaperSelectedOnOldPickerImpl_(
                  e.target.result, layout, true /*generateThumbnail=*/,
                  fileName,
                  function(thumbnail) {
                    self.saveCustomWallpaperToSyncFS_(
                        fileName, layout, e.target.result, thumbnail,
                        errorHandler);
                  },
                  function() {
                    self.removeCustomWallpaper(fileName);
                    errorHandler();
                  });
            });
          });
          fileWriter.addEventListener('error', errorHandler);
          fileWriter.write(file);
        }, errorHandler);
      }, errorHandler);
    };
    self.wallpaperDirs_.getDirectory(
        Constants.WallpaperDirNameEnum.ORIGINAL, success, errorHandler);
  };
  setSelectedFile(files[0], layout, new Date().getTime().toString());
};

/**
 * Removes wallpaper and thumbnail with fileName from FileSystem.
 * @param {string} fileName The file name of wallpaper and thumbnail to be
 *     removed.
 */
WallpaperManager.prototype.removeCustomWallpaper = function(fileName) {
  var errorHandler = this.onFileSystemError_.bind(this);
  var self = this;
  var removeFile = function(fileName) {
    var success = function(dirEntry) {
      dirEntry.getFile(fileName, {create: false}, function(fileEntry) {
        fileEntry.remove(function() {
          WallpaperUtil.deleteWallpaperFromSyncFS(fileName);
        }, errorHandler);
      }, errorHandler);
    };

    // Removes copy of original.
    self.wallpaperDirs_.getDirectory(
        Constants.WallpaperDirNameEnum.ORIGINAL, success, errorHandler);

    // Removes generated thumbnail.
    self.wallpaperDirs_.getDirectory(
        Constants.WallpaperDirNameEnum.THUMBNAIL, success, errorHandler);
  };
  removeFile(fileName);
};

/**
 * TODO(crbug.com/787134): Delete the method after the old picker is deprecated.
 *
 * Implementation of |setCustomWallpaperSelectedOnOldPicker_|.
 * @param {ArrayBuffer} wallpaper The binary representation of wallpaper.
 * @param {string} layout The user selected wallpaper layout.
 * @param {boolean} generateThumbnail True if need to generate thumbnail.
 * @param {string} fileName The unique file name of wallpaper.
 * @param {function(thumbnail):void} success Success callback. If
 *     generateThumbnail is true, the callback parameter should have the
 *     generated thumbnail.
 * @param {function(e):void} failure Failure callback. Called when there is an
 *     error from FileSystem.
 * @private
 */
WallpaperManager.prototype.setCustomWallpaperSelectedOnOldPickerImpl_ =
    function(wallpaper, layout, generateThumbnail, fileName, success, failure) {
  var onFinished = opt_thumbnail => {
    if (chrome.runtime.lastError != undefined &&
        chrome.runtime.lastError.message != str('canceledWallpaper')) {
      this.showError_(chrome.runtime.lastError.message);
      $('set-wallpaper-layout').disabled = true;
      failure();
    } else {
      success(opt_thumbnail);
    }
  };

  chrome.wallpaperPrivate.setCustomWallpaper(
      wallpaper, layout, generateThumbnail, fileName, false /*previewMode=*/,
      onFinished);
};

/**
 * Handles the layout setting change of custom wallpaper.
 */
WallpaperManager.prototype.onWallpaperLayoutChanged_ = function() {
  this.setCustomWallpaperLayout_(getSelectedLayout());
};

/**
 * Saves the custom wallpaper and thumbnail (if any) to the sync file system.
 * @param {string} fileName The file name of the wallpaper.
 * @param {string} layout The desired layout of the wallpaper.
 * @param {string} wallpaperData The data of the full-size wallpaper.
 * @param {?string} optThumbnailData The data of the thumbnail-size wallpaper.
 * @param {?function} optErrorCallbacka The error callback. Must be non-null if
 *     optThumbnailData is non-null.
 * @private
 */
WallpaperManager.prototype.saveCustomWallpaperToSyncFS_ = function(
    fileName, layout, wallpaperData, optThumbnailData, optErrorCallback) {
  WallpaperUtil.storeWallpaperToSyncFS(fileName, wallpaperData);
  if (!optThumbnailData)
    return;
  WallpaperUtil.storeWallpaperToSyncFS(
      fileName + Constants.CustomWallpaperThumbnailSuffix, optThumbnailData);

  var success = dirEntry => {
    dirEntry.getFile(fileName, {create: true}, fileEntry => {
      fileEntry.createWriter(fileWriter => {
        fileWriter.onwriteend = e => {
          $('set-wallpaper-layout').disabled = false;
          var wallpaperInfo = {
            baseURL: fileName,
            layout: layout,
            source: Constants.WallpaperSourceEnum.Custom,
            availableOffline: true
          };
          this.wallpaperGrid_.dataModel.splice(0, 0, wallpaperInfo);
          this.wallpaperGrid_.selectedItem = wallpaperInfo;
          this.onWallpaperChanged_(wallpaperInfo, fileName);
          WallpaperUtil.saveToLocalStorage(this.currentWallpaper_, layout);
        };

        fileWriter.onerror = optErrorCallback;
        fileWriter.write(WallpaperUtil.createPngBlob(optThumbnailData));
      }, optErrorCallback);
    }, optErrorCallback);
  };
  this.wallpaperDirs_.getDirectory(
      Constants.WallpaperDirNameEnum.THUMBNAIL, success, optErrorCallback);
};

/**
 * Implementation of |onWallpaperLayoutChanged_|. A wrapper of the
 * |setCustomWallpaperLayout| api.
 * @param {string} layout The user selected wallpaper layout.
 * @private
 */
WallpaperManager.prototype.setCustomWallpaperLayout_ = function(layout) {
  chrome.wallpaperPrivate.setCustomWallpaperLayout(layout, () => {
    if (chrome.runtime.lastError != undefined &&
        chrome.runtime.lastError.message != str('canceledWallpaper')) {
      this.showError_(chrome.runtime.lastError.message);
      this.removeCustomWallpaper(fileName);
      $('set-wallpaper-layout').disabled = true;
    } else {
      WallpaperUtil.saveToLocalStorage(this.currentWallpaper_, layout);
      this.onWallpaperChanged_(
          this.wallpaperGrid_.activeItem, this.currentWallpaper_);
    }
  });
};

/**
 * Handles UI changes based on whether surprise me is enabled.
 * @param enabled Whether surprise me is enabled.
 * @private
 */
WallpaperManager.prototype.onSurpriseMeStateChanged_ = function(enabled) {
  WallpaperUtil.setSurpriseMeCheckboxValue(enabled);
  $('categories-list').disabled = enabled;
  if (enabled)
    this.document_.body.removeAttribute('surprise-me-disabled');
  else
    this.document_.body.setAttribute('surprise-me-disabled', '');

  // The wallpaper grid on the new picker contains UI elements that should
  // not be disabled.
  if (!this.useNewWallpaperPicker_)
    $('wallpaper-grid').disabled = enabled;
};

/**
 * Handles user clicking on a different category.
 */
WallpaperManager.prototype.onCategoriesChange_ = function() {
  var categoriesList = this.categoriesList_;
  var selectedIndex = categoriesList.selectionModel.selectedIndex;
  if (selectedIndex == -1)
    return;
  var selectedListItem = categoriesList.getListItemByIndex(selectedIndex);
  var bar = $('bar');
  bar.style.left = selectedListItem.offsetLeft + 'px';
  bar.style.width = selectedListItem.offsetWidth + 'px';

  var wallpapersDataModel = new cr.ui.ArrayDataModel([]);
  var selectedItem = null;
  if (selectedListItem.custom) {
    if (this.useNewWallpaperPicker_) {
      chrome.wallpaperPrivate.getLocalImagePaths(localImagePaths => {
        // Show a 'no images' message to user if there's no local image.
        this.document_.body.classList.toggle(
            'no-images', localImagePaths.length == 0);
        var wallpapersDataModel = new cr.ui.ArrayDataModel([]);
        for (var imagePath of localImagePaths) {
          var wallpaperInfo = {
            // The absolute file path, used for retrieving the image data
            // if user chooses to set this wallpaper.
            filePath: imagePath,
            // Used as the file name when saving the wallpaper to local and
            // sync storage, which only happens after user chooses to set
            // this wallpaper. The name 'baseURL' is for consistency with
            // the old wallpaper picker.
            // TODO(crbug.com/812085): Rename it to fileName after the new
            // wallpaper picker is enabled by default.
            baseURL: new Date().getTime().toString(),
            layout: Constants.WallpaperThumbnailDefaultLayout,
            source: Constants.WallpaperSourceEnum.Custom,
            availableOffline: true,
            collectionName: str('customCategoryLabel')
          };
          wallpapersDataModel.push(wallpaperInfo);
        }
        // Display the images.
        this.wallpaperGrid_.dataModel = wallpapersDataModel;
      });
      return;
    }

    this.document_.body.setAttribute('custom', '');
    var errorHandler = this.onFileSystemError_.bind(this);
    var toArray = function(list) {
      return Array.prototype.slice.call(list || [], 0);
    };

    var self = this;
    var processResults = function(entries) {
      for (var i = 0; i < entries.length; i++) {
        var entry = entries[i];
        var wallpaperInfo = {
          // Set wallpaperId to null to avoid duplicate thumbnail images,
          // see crbug.com/506135 for details.
          wallpaperId: null,
          baseURL: entry.name,
          // The layout will be replaced by the actual value saved in
          // local storage when requested later. Layout is not important
          // for constructing thumbnails grid, we use the default here
          // to speed up the process of constructing. So we do not need to
          // wait for fetching correct layout.
          layout: Constants.WallpaperThumbnailDefaultLayout,
          source: Constants.WallpaperSourceEnum.Custom,
          availableOffline: true
        };
        wallpapersDataModel.push(wallpaperInfo);
      }
      if (loadTimeData.getBoolean('isOEMDefaultWallpaper')) {
        var oemDefaultWallpaperElement = {
          wallpaperId: null,
          baseURL: 'OemDefaultWallpaper',
          layout: Constants.WallpaperThumbnailDefaultLayout,
          source: Constants.WallpaperSourceEnum.OEM,
          availableOffline: true
        };
        wallpapersDataModel.push(oemDefaultWallpaperElement);
      }
      for (var i = 0; i < wallpapersDataModel.length; i++) {
        // For custom wallpapers, the file name of |currentWallpaper_|
        // includes the first directory level (corresponding to user id hash).
        if (getBaseName(self.currentWallpaper_) ==
            wallpapersDataModel.item(i).baseURL) {
          selectedItem = wallpapersDataModel.item(i);
        }
      }
      var lastElement = {
        baseURL: '',
        layout: '',
        source: Constants.WallpaperSourceEnum.AddNew,
        availableOffline: true
      };
      wallpapersDataModel.push(lastElement);
      self.wallpaperGrid_.dataModel = wallpapersDataModel;
      if (selectedItem) {
        self.wallpaperGrid_.selectedItem = selectedItem;
        self.wallpaperGrid_.activeItem = selectedItem;
      }
    };

    var success = function(dirEntry) {
      var dirReader = dirEntry.createReader();
      var entries = [];
      // All of a directory's entries are not guaranteed to return in a single
      // call.
      var readEntries = function() {
        dirReader.readEntries(function(results) {
          if (!results.length) {
            processResults(entries.sort());
          } else {
            entries = entries.concat(toArray(results));
            readEntries();
          }
        }, errorHandler);
      };
      readEntries();  // Start reading dirs.
    };
    this.wallpaperDirs_.getDirectory(
        Constants.WallpaperDirNameEnum.ORIGINAL, success, errorHandler);
  } else {
    this.document_.body.removeAttribute('custom');

    if (this.useNewWallpaperPicker_ && this.collectionsInfo_) {
      this.showCollection_(selectedIndex);
      return;
    }

    // Need this check for test purpose.
    var numOnlineWallpaper = (this.enableOnlineWallpaper_ && this.manifest_) ?
        this.manifest_.wallpaper_list.length :
        0;
    for (var i = 0; i < numOnlineWallpaper; i++) {
      if (selectedIndex == AllCategoryIndex ||
          this.manifest_.wallpaper_list[i].categories.indexOf(
              selectedIndex - OnlineCategoriesOffset) != -1) {
        var wallpaperInfo = {
          wallpaperId: i,
          baseURL: this.manifest_.wallpaper_list[i].base_url,
          layout: this.manifest_.wallpaper_list[i].default_layout,
          source: Constants.WallpaperSourceEnum.Online,
          availableOffline: false,
          author: this.manifest_.wallpaper_list[i].author,
          authorWebsite: this.manifest_.wallpaper_list[i].author_website,
          dynamicURL: this.manifest_.wallpaper_list[i].dynamic_url
        };
        var fileName =
            getBaseName(wallpaperInfo.baseURL) + str('highResolutionSuffix');
        if (this.downloadedListMap_ &&
            this.downloadedListMap_.hasOwnProperty(encodeURI(fileName))) {
          wallpaperInfo.availableOffline = true;
        }
        wallpapersDataModel.push(wallpaperInfo);
        var url = this.manifest_.wallpaper_list[i].base_url +
            str('highResolutionSuffix');
        if (url == this.currentWallpaper_) {
          selectedItem = wallpaperInfo;
        }
      }
    }
    this.wallpaperGrid_.dataModel = wallpapersDataModel;
    if (selectedItem) {
      this.wallpaperGrid_.selectedItem = selectedItem;
      this.wallpaperGrid_.activeItem = selectedItem;
    }
  }
};

})();
