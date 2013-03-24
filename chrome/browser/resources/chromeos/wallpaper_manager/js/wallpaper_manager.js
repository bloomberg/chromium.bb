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
  this.backgroundPage_ = null;
  this.wallpaperDirs_ = WallpaperDirectories.getInstance();
  this.fetchBackgroundPage_();
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
   * Fetches the background page of the app.
   */
  WallpaperManager.prototype.fetchBackgroundPage_ = function() {
    var self = this;
    chrome.runtime.getBackgroundPage(function(backgroundPage) {
      self.backgroundPage_ = backgroundPage;
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
   * Checks if the backgroud page is fetched and starts task to fetch it if not.
   * @param {function} callback The callback function after successfully fetched
   *     background page.
   * @return {boolean} True if background page has been fetched.
   */
  WallpaperManager.prototype.backgroundPageFetched_ = function(callback) {
    var self = this;
    if (!this.backgroundPage_) {
      chrome.runtime.getBackgroundPage(function(backgroundPage) {
        if (!backgroundPage) {
          // This should never happen.
          console.error('Failed to get background page for wallpaper picker.');
          return;
        }
        self.backgroundPage_ = backgroundPage;
        callback();
      });
      return false;
    }
    return true;
  };

  /**
   * Sets manifest loaded from server. Called after manifest is successfully
   * loaded.
   * @param {object} manifest The parsed manifest file.
   */
  WallpaperManager.prototype.onLoadManifestSuccess_ = function(manifest) {
    // Background page should have already been set at this point. Adding a
    // check here to make it extra safe.
    if (!this.backgroundPageFetched_(this.onLoadManifestSuccess_.bind(this)))
      return;

    this.manifest_ = manifest;
    var items = {};
    items[this.backgroundPage_.AccessManifestKey] = manifest;
    this.storage_.set(items, function() {});
    this.initDom_();
  };

  // Sets manifest to previously saved object if any and shows connection error.
  // Called after manifest failed to load.
  WallpaperManager.prototype.onLoadManifestFailed_ = function() {
    if (!this.backgroundPageFetched_(this.onLoadManifestFailed_.bind(this)))
      return;

    var accessManifestKey = this.backgroundPage_.AccessManifestKey;
    var self = this;
    this.storage_.get(accessManifestKey, function(items) {
      self.manifest_ = items[accessManifestKey] ? items[accessManifestKey] : {};
      self.showError_(str('connectionFailed'));
      self.initDom_();
      $('wallpaper-grid').classList.add('image-picker-offline');
    });
  };

  /**
   * Toggle surprise me feature of wallpaper picker.
   * @private
   */
  WallpaperManager.prototype.toggleSurpriseMe_ = function() {
    var checkbox = $('surprise-me').querySelector('#checkbox');
    if (checkbox.classList.contains('checked')) {
      checkbox.classList.remove('checked');
      this.backgroundPage_.SurpriseWallpaper.getInstance().disableSurpriseMe(
          function() {
            // Disables thumbnails grid and category selection list.
            $('categories-list').disabled = false;
            $('wallpaper-grid').disabled = false;
          }, function() {
            checkbox.classList.add('checked');
            // TODO(bshe): show error message to user.
          });
    } else {
      checkbox.classList.add('checked');
      this.backgroundPage_.SurpriseWallpaper.getInstance().enableSurpriseMe(
          function() {
            // Disables thumbnails grid and category selection list.
            $('categories-list').disabled = true;
            $('wallpaper-grid').disabled = true;
          }, function() {
            checkbox.classList.remove('checked');
            // TODO(bshe): show error message to user.
      });
    }
  };

  /**
   * One-time initialization of various DOM nodes.
   */
  WallpaperManager.prototype.initDom_ = function() {
    i18nTemplate.process(this.document_, loadTimeData);
    this.initCategoriesList_();
    this.initThumbnailsGrid_();
    this.presetCategory_();

    $('surprise-me').addEventListener('click',
                                      this.toggleSurpriseMe_.bind(this));
    $('file-selector').addEventListener(
        'change', this.onFileSelectorChanged_.bind(this));
    $('set-wallpaper-layout').addEventListener(
        'change', this.onWallpaperLayoutChanged_.bind(this));
    var self = this;
    var accessSurpriseMeEnabledKey =
        this.backgroundPage_.AccessSurpriseMeEnabledKey;
    this.storage_.get(accessSurpriseMeEnabledKey, function(items) {
      if (items[accessSurpriseMeEnabledKey]) {
        self.toggleSurpriseMe_();
      }
    });

    window.addEventListener('offline', function() {
      chrome.wallpaperPrivate.getOfflineWallpaperList(
          wallpapers.WallpaperSourceEnum.Online, function(lists) {
        if (!self.downloadedListMap_)
          self.downloadedListMap_ = {};
        for (var i = 0; i < lists.length; i++)
          self.downloadedListMap_[lists[i]] = true;
        var thumbnails = self.document_.querySelectorAll('.thumbnail');
        for (var i = 0; i < thumbnails.length; i++) {
          var thumbnail = thumbnails[i];
          var url = self.wallpaperGrid_.dataModel.item(i).baseURL;
          var fileName = url.substring(url.lastIndexOf('/') + 1) +
              this.backgroundPage_.HighResolutionSuffix;
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
    $('close-wallpaper-selection').addEventListener('click', function() {
      $('wallpaper-selection-container').hidden = true;
      $('set-wallpaper-layout').disabled = true;
    });

    this.onResize_();
  };

  /**
   * Preset to the category which contains current wallpaper.
   */
  WallpaperManager.prototype.presetCategory_ = function() {
    this.currentWallpaper_ = str('currentWallpaper');
    // The currentWallpaper_ is either a url contains HightResolutionSuffix or a
    // custom wallpaper file name converted from an integer value represent
    // time (e.g., 13006377367586070).
    if (this.currentWallpaper_ &&
        this.currentWallpaper_.indexOf(
            this.backgroundPage_.HighResolutionSuffix) == -1) {
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
      if (self.currentWallpaper_) {
        for (var key in self.manifest_.wallpaper_list) {
          var url = self.manifest_.wallpaper_list[key].base_url +
              self.backgroundPage_.HighResolutionSuffix;
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
      presetCategoryInner_();
    } else {
      // If device is offline, gets the available offline wallpaper list first.
      // Wallpapers which are not in the list will display a grayscaled
      // thumbnail.
      chrome.wallpaperPrivate.getOfflineWallpaperList(
          wallpapers.WallpaperSourceEnum.Online, function(lists) {
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
        this.onThumbnailSelectionChanged_.bind(this));
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
    * @param {{baseURL: string, layout: string, source: string,
    *          availableOffline: boolean, opt_dynamicURL: string,
    *          opt_author: string, opt_authorWebsite: string}}
    *     selectedItem the selected item in WallpaperThumbnailsGrid's data
    *     model.
    */
  WallpaperManager.prototype.setSelectedWallpaper_ = function(selectedItem) {
    var self = this;
    switch (selectedItem.source) {
      case wallpapers.WallpaperSourceEnum.Custom:
        var errorHandler = this.onFileSystemError_.bind(this);
        var setActive = function() {
          self.wallpaperGrid_.activeItem = selectedItem;
          self.currentWallpaper_ = selectedItem.baseURL;
        };
        var success = function(dirEntry) {
          dirEntry.getFile(selectedItem.baseURL, {create: false},
                           function(fileEntry) {
            fileEntry.file(function(file) {
              var reader = new FileReader();
              reader.readAsArrayBuffer(file);
              reader.addEventListener('error', errorHandler);
              reader.addEventListener('load', function(e) {
                self.setCustomWallpaper(e.target.result,
                                        selectedItem.layout,
                                        false, selectedItem.baseURL,
                                        setActive, errorHandler);
              });
            }, errorHandler);
          }, errorHandler);
        }
        this.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL,
                                         success, errorHandler);
        break;
      case wallpapers.WallpaperSourceEnum.Online:
        var wallpaperURL = selectedItem.baseURL +
            this.backgroundPage_.HighResolutionSuffix;
        var selectedGridItem = this.wallpaperGrid_.getListItem(selectedItem);

        chrome.wallpaperPrivate.setWallpaperIfExists(wallpaperURL,
                                                     selectedItem.layout,
                                                     selectedItem.source,
                                                     function(exists) {
          if (exists) {
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
        break;
      default:
        console.error('Unsupported wallpaper source.');
    }
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
    if (!item || item.source != wallpapers.WallpaperSourceEnum.Custom)
      return;
    console.error(item.baseURL);
    if (item.baseURL == this.currentWallpaper_)
      item = this.wallpaperGrid_.dataModel.item(--oldestIndex);
    if (item) {
      this.removeCustomWallpaper(item.baseURL);
      this.wallpaperGrid_.dataModel.splice(oldestIndex, 1);
    }
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
   * Handles click on a different thumbnail in wallpaper grid.
   */
  WallpaperManager.prototype.onThumbnailSelectionChanged_ = function() {
    var selectedItem = this.wallpaperGrid_.selectedItem;
    if (selectedItem && selectedItem.source == 'ADDNEW')
      return;

    if (selectedItem && selectedItem.baseURL &&
        !this.wallpaperGrid_.inProgramSelection) {
      if (selectedItem.source == wallpapers.WallpaperSourceEnum.Custom) {
        var items = {};
        var key = selectedItem.baseURL;
        var self = this;
        this.storage_.get(key, function(items) {
          selectedItem.layout = items[key] ? items[key] : 'CENTER_CROPPED';
          self.setSelectedWallpaper_(selectedItem);
        });
      } else {
        this.setSelectedWallpaper_(selectedItem);
      }
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
                                           selectedItem.source,
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
    if (!files[0].type.match('image/jpeg')) {
      this.showError_(str('invalidWallpaper'));
      return;
    }
    var layout = getSelectedLayout();
    var self = this;
    var errorHandler = this.onFileSystemError_.bind(this);
    var setSelectedFile = function(file, layout, fileName) {
      var saveThumbnail = function(thumbnail) {
        var success = function(dirEntry) {
          dirEntry.getFile(fileName, {create: true}, function(fileEntry) {
            fileEntry.createWriter(function(fileWriter) {
              fileWriter.onwriteend = function(e) {
                $('set-wallpaper-layout').disabled = false;
                var wallpaperInfo = {
                  baseURL: fileName,
                  layout: layout,
                  source: wallpapers.WallpaperSourceEnum.Custom,
                  availableOffline: true
                };
                self.currentWallpaper_ = fileName;
                var items = {};
                items[self.currentWallpaper_] = layout;
                self.storage_.set(items, function() {});
                self.wallpaperGrid_.dataModel.splice(0, 0, wallpaperInfo);
                self.wallpaperGrid_.selectedItem = wallpaperInfo;
                self.wallpaperGrid_.activeItem = wallpaperInfo;
              };

              fileWriter.onerror = errorHandler;

              var blob = new Blob([new Int8Array(thumbnail)],
                                  {'type' : 'image\/jpeg'});
              fileWriter.write(blob);
            }, errorHandler);
          }, errorHandler);
        };
        self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.THUMBNAIL,
            success, errorHandler);
      };

      var success = function(dirEntry) {
        dirEntry.getFile(fileName, {create: true}, function(fileEntry) {
          fileEntry.createWriter(function(fileWriter) {
            fileWriter.addEventListener('writeend', function(e) {
              var reader = new FileReader();
              reader.readAsArrayBuffer(file);
              reader.addEventListener('error', errorHandler);
              reader.addEventListener('load', function(e) {
                self.setCustomWallpaper(e.target.result, layout, true, fileName,
                                        saveThumbnail, function() {
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
      self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL, success,
                                       errorHandler);
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
          }, errorHandler);
        }, errorHandler);
      }

      // Removes copy of original.
      self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL, success,
                                       errorHandler);

      // Removes generated thumbnail.
      self.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.THUMBNAIL, success,
                                       errorHandler);
    };
    removeFile(fileName);
  };

  /**
   * Sets current wallpaper and generate thumbnail if generateThumbnail is true.
   * @param {ArrayBuffer} wallpaper The binary representation of wallpaper.
   * @param {string} layout The user selected wallpaper layout.
   * @param {boolean} generateThumbnail True if need to generate thumbnail.
   * @param {string} fileName The unique file name of wallpaper.
   * @param {function(thumbnail):void} success Success callback. If
   *     generateThumbnail is true, the callback parameter should have the
   *     generated thumbnail.
   * @param {function(e):void} failure Failure callback. Called when there is an
   *     error from FileSystem.
   */
  WallpaperManager.prototype.setCustomWallpaper = function(wallpaper,
                                                           layout,
                                                           generateThumbnail,
                                                           fileName,
                                                           success,
                                                           failure) {
    var self = this;
    var onFinished = function(opt_thumbnail) {
      if (chrome.runtime.lastError != undefined) {
        self.showError_(chrome.runtime.lastError.message);
        $('set-wallpaper-layout').disabled = true;
      } else {
        success(opt_thumbnail);
      }
    };

    chrome.wallpaperPrivate.setCustomWallpaper(wallpaper, layout,
                                               generateThumbnail,
                                               fileName, onFinished);
  };

  /**
   * Sets wallpaper finished. Displays error message if any.
   * @param {WallpaperThumbnailsGridItem=} opt_selectedGridItem The wallpaper
   *     thumbnail grid item. It extends from cr.ui.ListItem.
   * @param {{baseURL: string, layout: string, source: string,
   *          availableOffline: boolean, opt_dynamicURL: string,
   *          opt_author: string, opt_authorWebsite: string}=}
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
    var layout = getSelectedLayout();
    var self = this;
    chrome.wallpaperPrivate.setCustomWallpaperLayout(layout, function() {
      if (chrome.runtime.lastError != undefined) {
        self.showError_(chrome.runtime.lastError.message);
        self.removeCustomWallpaper(fileName);
        $('set-wallpaper-layout').disabled = true;
      } else {
        var items = {};
        items[self.currentWallpaper_] = layout;
        self.storage_.set(items, function() {});
      }
    });
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
    var selectedItem;
    if (selectedListItem.custom) {
      $('online-wallpaper-attribute').hidden = true;
      var errorHandler = this.onFileSystemError_.bind(this);
      var toArray = function(list) {
        return Array.prototype.slice.call(list || [], 0);
      }

      var self = this;
      var processResults = function(entries) {
        for (var i = 0; i < entries.length; i++) {
          var entry = entries[i];
          var wallpaperInfo = {
                baseURL: entry.name,
                // The layout will be replaced by the actual value saved in
                // local storage when requested later. Layout is not important
                // for constructing thumbnails grid, we use CENTER_CROPPED here
                // to speed up the process of constructing. So we do not need to
                // wait for fetching correct layout.
                layout: 'CENTER_CROPPED',
                source: wallpapers.WallpaperSourceEnum.Custom,
                availableOffline: true
          };
          if (self.currentWallpaper_ == entry.name)
            selectedItem = wallpaperInfo;
          wallpapersDataModel.push(wallpaperInfo);
        }
        var lastElement = {
            baseURL: '',
            layout: '',
            source: 'ADDNEW',
            availableOffline: true
        };
        wallpapersDataModel.push(lastElement);
        self.wallpaperGrid_.dataModel = wallpapersDataModel;
        self.wallpaperGrid_.selectedItem = selectedItem;
        self.wallpaperGrid_.activeItem = selectedItem;
      }

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
        readEntries(); // Start reading dirs.
      }
      this.wallpaperDirs_.getDirectory(WallpaperDirNameEnum.ORIGINAL,
                                       success, errorHandler);
    } else {
      $('online-wallpaper-attribute').hidden = false;
      for (var key in this.manifest_.wallpaper_list) {
        if (selectedIndex == AllCategoryIndex ||
            this.manifest_.wallpaper_list[key].categories.indexOf(
                selectedIndex - OnlineCategoriesOffset) != -1) {
          var wallpaperInfo = {
            baseURL: this.manifest_.wallpaper_list[key].base_url,
            layout: this.manifest_.wallpaper_list[key].default_layout,
            source: wallpapers.WallpaperSourceEnum.Online,
            availableOffline: false,
            author: this.manifest_.wallpaper_list[key].author,
            authorWebsite: this.manifest_.wallpaper_list[key].author_website,
            dynamicURL: this.manifest_.wallpaper_list[key].dynamic_url
          };
          var startIndex = wallpaperInfo.baseURL.lastIndexOf('/') + 1;
          var fileName = wallpaperInfo.baseURL.substring(startIndex) +
              this.backgroundPage_.HighResolutionSuffix;
          if (this.downloadedListMap_ &&
              this.downloadedListMap_.hasOwnProperty(encodeURI(fileName))) {
            wallpaperInfo.availableOffline = true;
          }
          wallpapersDataModel.push(wallpaperInfo);
          var url = this.manifest_.wallpaper_list[key].base_url +
              this.backgroundPage_.HighResolutionSuffix;
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
