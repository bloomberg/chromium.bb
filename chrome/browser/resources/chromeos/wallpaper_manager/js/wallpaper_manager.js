// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
  this.storage_ = chrome.storage.local;
  this.document_ = dialogDom.ownerDocument;
  this.selectedCategory = null;
  this.progressManager_ = new ProgressManager();
  this.customWallpaperData_ = null;
  this.currentWallpaper_ = null;
  this.wallpaperRequest_ = null;
  this.fetchManifest_();
}

// Anonymous 'namespace'.
// TODO(bshe): Get rid of anonymous namespace.
(function() {

  /**
   * Base URL of the manifest file.
   */
  /** @const */ var ManifestBaseURL = 'https://commondatastorage.googleapis.' +
      'com/chromeos-wallpaper-public/manifest_';

  /**
   * URL of the learn more page for wallpaper picker.
   */
  /** @const */ var LearnMoreURL =
      'https://support.google.com/chromeos/?p=wallpaper_fileerror&hl=' +
          navigator.language;

  /**
   * Suffix to append to baseURL if requesting high resoultion wallpaper.
   */
  /** @const */ var HighResolutionSuffix = '_high_resolution.jpg';

  /**
   * Key to access wallpaper manifest in chrome.local.storage.
   */
  /** @const */ var AccessManifestKey = 'wallpaper-picker-manifest-key';

  /**
   * Index of the All category. It is the first category in wallpaper picker.
   */
  /** @const */ var AllCategoryIndex = 0;

  /**
   * Index offset of categories parsed from manifest. The All category is added
   * before them. So the offset is 1.
   */
  /** @const */ var OnlineCategoriesOffset = 1;

  /**
   * Returns a translated string.
   *
   * Wrapper function to make dealing with translated strings more concise.
   * Equivilant to localStrings.getString(id).
   *
   * @param {string} id The id of the string to return.
   * @return {string} The translated string.
   */
  function str(id) {
    return loadTimeData.getString(id);
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
    var urls = [
        ManifestBaseURL + locale + '.json',
        // Fallback url. Use 'en' locale by default.
        ManifestBaseURL + 'en.json'];

    var asyncFetchManifestFromUrls = function(urls, func, successCallback,
                                              failureCallback) {
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
      asyncFetchManifestFromUrls(urls, fetchManifestAsync,
                                 this.onLoadManifestSuccess_.bind(this),
                                 this.onLoadManifestFailed_.bind(this));
    } else {
      // If device is offline, fetches manifest from local storage.
      // TODO(bshe): Always loading the offline manifest first and replacing
      // with the online one when available.
      this.onLoadManifestFailed_();
    }
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
    var items = {};
    items[AccessManifestKey] = manifest;
    this.storage_.set(items, function() {});
    this.initDom_();
  };

  // Sets manifest to previously saved object if any and shows connection error.
  // Called after manifest failed to load.
  WallpaperManager.prototype.onLoadManifestFailed_ = function() {
    var self = this;
    this.storage_.get(AccessManifestKey, function(items) {
      self.manifest_ = items[AccessManifestKey] ? items[AccessManifestKey] : {};
      self.showError_(str('connectionFailed'));
      self.initDom_();
      $('wallpaper-grid').classList.add('image-picker-offline');
    });
  };

  /**
   * One-time initialization of various DOM nodes.
   */
  WallpaperManager.prototype.initDom_ = function() {
    i18nTemplate.process(this.document_, loadTimeData);
    this.initCategoriesList_();
    this.initThumbnailsGrid_();
    this.presetCategory_();

    $('file-selector').addEventListener(
        'change', this.onFileSelectorChanged_.bind(this));
    $('set-wallpaper-layout').addEventListener(
        'change', this.onWallpaperLayoutChanged_.bind(this));
    var self = this;
    window.addEventListener('offline', function() {
      chrome.wallpaperPrivate.getOfflineWallpaperList('ONLINE',
                                                      function(lists) {
        if (!self.downloadedListMap_)
          self.downloadedListMap_ = {};
        for (var i = 0; i < lists.length; i++)
          self.downloadedListMap_[lists[i]] = true;
        var thumbnails = self.document_.querySelectorAll('.thumbnail');
        for (var i = 0; i < thumbnails.length; i++) {
          var thumbnail = thumbnails[i];
          var url = self.wallpaperGrid_.dataModel.item(i).baseURL;
          var fileName = url.substring(url.lastIndexOf('/') + 1) +
              HighResolutionSuffix;
          if (self.downloadedListMap_ &&
              self.downloadedListMap_.hasOwnProperty(encodeURI(fileName))) {
            thumbnail.offline = true;
          }
        }
      });
      $('wallpaper-grid').classList.add('image-picker-offline');
    });
    window.addEventListener('online', function() {
      self.downloadedListMap_ = null;
      $('wallpaper-grid').classList.remove('image-picker-offline');
    });
    $('close').addEventListener('click', function() {window.close()});
    this.document_.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));
    $('learn-more').href = LearnMoreURL;
    $('close-error').addEventListener('click', function() {
      $('error-container').hidden = true;
    });

    this.onResize_();
  };

  /**
   * Preset to the category which contains current wallpaper.
   */
  WallpaperManager.prototype.presetCategory_ = function() {
    this.currentWallpaper_ = str('currentWallpaper');
    if (this.currentWallpaper_ && this.currentWallpaper_ == 'CUSTOM') {
      // Custom is the last one in the categories list.
      this.categoriesList_.selectionModel.selectedIndex =
          this.categoriesList_.dataModel.length - 1;
      return;
    }
    var self = this;
    var presetCategoryInner_ = function() {
      // Selects the first category in the categories list of current
      // wallpaper as the default selected category when showing wallpaper
      // picker UI.
      var presetCategory = AllCategoryIndex;
      for (var key in self.manifest_.wallpaper_list) {
        var url = self.manifest_.wallpaper_list[key].base_url +
            HighResolutionSuffix;
        if (url.indexOf(self.currentWallpaper_) != -1)
          presetCategory = self.manifest_.wallpaper_list[key].categories[0] +
              OnlineCategoriesOffset;
      }
      self.categoriesList_.selectionModel.selectedIndex = presetCategory;
    };
    if (navigator.onLine) {
      presetCategoryInner_();
    } else {
      // If device is offline, gets the available offline wallpaper list first.
      // Wallpapers which are not in the list will display a grayscaled
      // thumbnail.
      chrome.wallpaperPrivate.getOfflineWallpaperList('ONLINE',
                                                      function(lists) {
        if (!self.downloadedListMap_)
          self.downloadedListMap_ = {};
        for (var i = 0; i < lists.length; i++)
          self.downloadedListMap_[lists[i]] = true;
        presetCategoryInner_();
      });
    }
  };

  /**
   * Constructs the thumbnails grid.
   */
  WallpaperManager.prototype.initThumbnailsGrid_ = function() {
    this.wallpaperGrid_ = $('wallpaper-grid');
    wallpapers.WallpaperThumbnailsGrid.decorate(this.wallpaperGrid_);

    this.wallpaperGrid_.addEventListener('change',
        this.onThumbnailClicked_.bind(this));
    this.wallpaperGrid_.addEventListener('dblclick', this.onClose_.bind(this));
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
   * Sets wallpaper to the corresponding wallpaper of selected thumbnail.
   */
  WallpaperManager.prototype.onThumbnailClicked_ = function() {
    var selectedItem = this.wallpaperGrid_.selectedItem;
    if (selectedItem && selectedItem.dynamicURL &&
        !this.wallpaperGrid_.inProgramSelection) {
      var wallpaperURL = selectedItem.baseURL + HighResolutionSuffix;
      var selectedGridItem = this.wallpaperGrid_.getListItem(selectedItem);
      var self = this;

      chrome.wallpaperPrivate.setWallpaperIfExist(wallpaperURL,
                                                  selectedItem.layout,
                                                  'ONLINE',
                                                  function() {
        if (chrome.runtime.lastError == undefined) {
          self.currentWallpaper_ = wallpaperURL;
          self.wallpaperGrid_.activeItem = selectedItem;
          return;
        }

        // Falls back to request wallpaper from server.
        if (self.wallpaperRequest_)
          self.wallpaperRequest_.abort();

        self.wallpaperRequest_ = new XMLHttpRequest();
        self.wallpaperRequest_.open('GET', wallpaperURL, true);
        self.wallpaperRequest_.responseType = 'arraybuffer';
        self.progressManager_.reset(self.wallpaperRequest_, selectedGridItem);
        self.wallpaperRequest_.send(null);
        self.wallpaperRequest_.addEventListener('load', function(e) {
          if (self.wallpaperRequest_.status === 200) {
            var image = self.wallpaperRequest_.response;
            chrome.wallpaperPrivate.setWallpaper(
                image,
                selectedItem.layout,
                wallpaperURL,
                self.onFinished_.bind(self, selectedGridItem, selectedItem));
            self.currentWallpaper_ = wallpaperURL;
          } else {
            self.progressManager_.hideProgressBar(selectedGridItem);
            self.showError_(str('downloadFailed'));
          }
          self.wallpaperRequest_ = null;
        });
        self.wallpaperRequest_.addEventListener('error', function() {
          self.showError_(str('downloadFailed'));
        });
      });
    }
    this.setWallpaperAttribution_(selectedItem);
  };

  /**
   * Set attributions of wallpaper with given URL. If URL is not valid, clear
   * the attributions.
   * @param {{baseURL: string, dynamicURL: string, layout: string,
   *          author: string, authorWebsite: string, availableOffline: boolean}}
   *     selectedItem selected wallpaper item in grid.
   * @private
   */
  WallpaperManager.prototype.setWallpaperAttribution_ = function(selectedItem) {
    if (selectedItem) {
      $('author-name').textContent = selectedItem.author;
      $('author-website').textContent = $('author-website').href =
          selectedItem.authorWebsite;
      chrome.wallpaperPrivate.getThumbnail(selectedItem.baseURL,
                                           function(data) {
        var img = $('attribute-image');
        if (data) {
          var blob = new Blob([new Int8Array(data)], {'type' : 'image\/png'});
          img.src = window.URL.createObjectURL(blob);
          img.addEventListener('load', function(e) {
            window.URL.revokeObjectURL(this.src);
          });
        } else {
          img.src = '';
        }
      });
      $('wallpaper-attribute').hidden = false;
      $('attribute-image').hidden = false;
      return;
    }
    $('wallpaper-attribute').hidden = true;
    $('attribute-image').hidden = true;
    $('author-name').textContent = '';
    $('author-website').textContent = $('author-website').href = '';
    $('attribute-image').src = '';
  };

  /**
   * Resize thumbnails grid and categories list to fit the new window size.
   */
  WallpaperManager.prototype.onResize_ = function() {
    this.wallpaperGrid_.redraw();
    this.categoriesList_.redraw();
  };

  /**
   * Constructs the categories list.
   */
  WallpaperManager.prototype.initCategoriesList_ = function() {
    this.categoriesList_ = $('categories-list');
    cr.ui.List.decorate(this.categoriesList_);
    // cr.ui.list calculates items in view port based on client height and item
    // height. However, categories list is displayed horizontally. So we should
    // not calculate visible items here. Sets autoExpands to true to show every
    // item in the list.
    // TODO(bshe): Use ul to replace cr.ui.list for category list.
    this.categoriesList_.autoExpands = true;

    var self = this;
    this.categoriesList_.itemConstructor = function(entry) {
      return self.renderCategory_(entry);
    };

    this.categoriesList_.selectionModel = new cr.ui.ListSingleSelectionModel();
    this.categoriesList_.selectionModel.addEventListener(
        'change', this.onCategoriesChange_.bind(this));

    var categoriesDataModel = new cr.ui.ArrayDataModel([]);
    // Adds all category as first category.
    categoriesDataModel.push(str('allCategoryLabel'));
    for (var key in this.manifest_.categories) {
      categoriesDataModel.push(this.manifest_.categories[key]);
    }
    // Adds custom category as last category.
    categoriesDataModel.push(str('customCategoryLabel'));
    this.categoriesList_.dataModel = categoriesDataModel;
  };

  /**
   * Constructs the element in categories list.
   * @param {string} entry Text content of a category.
   */
  WallpaperManager.prototype.renderCategory_ = function(entry) {
    var li = this.document_.createElement('li');
    cr.defineProperty(li, 'custom', cr.PropertyKind.BOOL_ATTR);
    li.custom = (entry == str('customCategoryLabel'));
    cr.defineProperty(li, 'lead', cr.PropertyKind.BOOL_ATTR);
    cr.defineProperty(li, 'selected', cr.PropertyKind.BOOL_ATTR);
    var div = this.document_.createElement('div');
    div.textContent = entry;
    li.appendChild(div);
    return li;
  };

  /**
   * Handles the custom wallpaper which user selected from file manager. Called
   * when users select a file.
   */
  WallpaperManager.prototype.onFileSelectorChanged_ = function() {
    var files = $('file-selector').files;
    if (files.length != 1)
      console.error('More than one files are selected or no file selected');
    var file = files[0];
    if (!file.type.match('image/jpeg')) {
      this.showError_(str('invalidWallpaper'));
      return;
    }
    var reader = new FileReader();
    reader.readAsArrayBuffer(files[0]);
    var self = this;
    reader.addEventListener('error', function(e) {
      self.showError_(str('accessFileFailure'));
    });
    reader.addEventListener('load', function(e) {
      self.customWallpaperData_ = e.target.result;
      self.refreshWallpaper_(self.customWallpaperData_);
    });
    this.generateThumbnail_(files[0]);
  };

  /**
   * Refreshes the custom wallpaper with the current selected layout.
   * @param {ArrayBuffer} customWallpaper The raw wallpaper file data.
   */
  WallpaperManager.prototype.refreshWallpaper_ = function(customWallpaper) {
    var setWallpaperLayout = $('set-wallpaper-layout');
    var layout =
        setWallpaperLayout.options[setWallpaperLayout.selectedIndex].value;
    chrome.wallpaperPrivate.setCustomWallpaper(customWallpaper,
                                               layout,
                                               this.onFinished_.bind(this));
    this.currentWallpaper_ = 'CUSTOM';
  };

  /**
   * Sets wallpaper finished. Displays error message in butter bar if any.
   * @param {WallpaperThumbnailsGridItem=} opt_selectedGridItem The wallpaper
   *     thumbnail grid item. It extends from cr.ui.ListItem.
   * @param {{baseURL: string, dynamicURL: string, layout: string,
   *          author: string, authorWebsite: string,
   *          availableOffline: boolean}=}
   *     opt_selectedItem the selected item in WallpaperThumbnailsGrid's data
   *     model.
   */
  WallpaperManager.prototype.onFinished_ = function(opt_selectedGridItem,
                                                    opt_selectedItem) {
    if (opt_selectedGridItem)
      this.progressManager_.hideProgressBar(opt_selectedGridItem);

    if (chrome.runtime.lastError != undefined) {
      this.showError_(chrome.runtime.lastError.message);
    } else if (opt_selectedItem) {
      this.wallpaperGrid_.activeItem = opt_selectedItem;
    }
  };

  /**
   * Handles the layout setting change of custom wallpaper.
   */
  WallpaperManager.prototype.onWallpaperLayoutChanged_ = function() {
    var setWallpaperLayout = $('set-wallpaper-layout');
    var layout =
        setWallpaperLayout.options[setWallpaperLayout.selectedIndex].value;
     chrome.wallpaperPrivate.setCustomWallpaperLayout(layout,
         this.onFinished_.bind(this));
  };

  /**
   * Generates a thumbnail of user selected image file.
   * @param {Object} file The file user selected from file manager.
   */
  WallpaperManager.prototype.generateThumbnail_ = function(file) {
    var img = $('preview');
    img.file = file;
    var reader = new FileReader();
    reader.addEventListener('load', function(e) {
      img.src = e.target.result;
    });
    reader.readAsDataURL(file);
  };

  /**
   * Toggle visibility of custom container and category container.
   * @param {boolean} showCustom True if display custom container and hide
   *     category container.
   */
  WallpaperManager.prototype.showCustomContainer_ = function(showCustom) {
    $('category-container').hidden = showCustom;
    $('custom-container').hidden = !showCustom;
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

    if (selectedListItem.custom) {
      this.showCustomContainer_(true);
    } else {
      this.showCustomContainer_(false);
      var selectedItem;
      var wallpapersDataModel = new cr.ui.ArrayDataModel([]);
      for (var key in this.manifest_.wallpaper_list) {
        if (selectedIndex == AllCategoryIndex ||
            this.manifest_.wallpaper_list[key].categories.indexOf(
                selectedIndex - OnlineCategoriesOffset) != -1) {
          var wallpaperInfo = {
            baseURL: this.manifest_.wallpaper_list[key].base_url,
            dynamicURL: this.manifest_.wallpaper_list[key].dynamic_url,
            layout: this.manifest_.wallpaper_list[key].default_layout,
            author: this.manifest_.wallpaper_list[key].author,
            authorWebsite: this.manifest_.wallpaper_list[key].author_website,
            availableOffline: false
          };
          var startIndex = wallpaperInfo.baseURL.lastIndexOf('/') + 1;
          var fileName = wallpaperInfo.baseURL.substring(startIndex) +
              HighResolutionSuffix;
          if (this.downloadedListMap_ &&
              this.downloadedListMap_.hasOwnProperty(encodeURI(fileName))) {
            wallpaperInfo.availableOffline = true;
          }
          wallpapersDataModel.push(wallpaperInfo);
          var url = this.manifest_.wallpaper_list[key].base_url +
              HighResolutionSuffix;
          if (url == this.currentWallpaper_) {
            selectedItem = wallpaperInfo;
          }
        }
      }
      this.wallpaperGrid_.dataModel = wallpapersDataModel;
      this.wallpaperGrid_.selectedItem = selectedItem;
      this.wallpaperGrid_.activeItem = selectedItem;
    }
  };

})();
