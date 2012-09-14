// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
  this.document_ = dialogDom.ownerDocument;
  this.selectedCategory = null;
  this.butterBar_ = new ButterBar(this.dialogDom_);
  this.customWallpaperData_ = null;
  this.fetchManifest_();
  this.initDom_();
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
   * Suffix to append to baseURL if requesting high resoultion wallpaper.
   */
  /** @const */ var HighResolutionSuffix = '_high_resolution.jpg';

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
   * Parses a string as manifest(JSON). Sets manifest to an empty object if a
   * parsing exception is catched.
   * @param {string} response The string to parse as JSON.
   */
  WallpaperManager.prototype.parseManifest_ = function(response) {
    try {
      this.manifest_ = JSON.parse(response);
    } catch (e) {
      this.butterBar_.showError_('Failed to parse manifest.');
      this.manifest_ = {};
    }
  };

  /**
   * Requests wallpaper manifest file from server.
   */
  WallpaperManager.prototype.fetchManifest_ = function() {
    var xhr = new XMLHttpRequest();
    var locale = navigator.language;
    xhr.open('GET', ManifestBaseURL + locale + '.json', false);
    xhr.send(null);
    // TODO(bshe): We should save the downloaded manifest to local disk. Other
    // components may want to use it (i.e. screen saver).
    if (xhr.status === 200) {
      this.parseManifest_(xhr.responseText);
    } else {
      // Fall back to en locale if current locale is not supported.
      xhr.open('GET', ManifestBaseURL + 'en.json', false);
      xhr.send(null);
      if (xhr.status === 200) {
        this.parseManifest_(xhr.responseText);
      } else {
        this.manifest_ = {};
        this.butterBar_.showError_('Failed to download manifest.');
      }
    }

    // TODO(bshe): Fall back to saved manifest if there is a problem fetching
    // manifest from server.
  };

  /**
   * One-time initialization of various DOM nodes.
   */
  WallpaperManager.prototype.initDom_ = function() {
    i18nTemplate.process(this.document_, loadTimeData);
    this.initCategoriesList_();
    this.initThumbnailsGrid_();

    var selectedWallpaper = str('selectedWallpaper');
    if (selectedWallpaper == 'CUSTOM') {
      // Custom is the last one in the categories list.
      this.categoriesList_.selectionModel.selectedIndex =
          this.categoriesList_.dataModel.length - 1;
    } else {
      // Selects the first category in the categories list of current wallpaper
      // as the default selected category when showing wallpaper picker UI.
      var firstCategory = 0;
      for (var key in this.manifest_.wallpaper_list) {
        var url = this.manifest_.wallpaper_list[key].base_url +
            HighResolutionSuffix;
        if (url.indexOf(selectedWallpaper) != -1) {
          firstCategory = this.manifest_.wallpaper_list[key].categories[0];
        }
      }
      this.categoriesList_.selectionModel.selectedIndex = firstCategory;
    }

    $('file-selector').addEventListener(
        'change', this.onFileSelectorChanged_.bind(this));

    $('set-wallpaper-layout').addEventListener(
        'change', this.onWallpaperLayoutChanged_.bind(this));

    this.dialogDom_.ownerDocument.defaultView.addEventListener(
        'resize', this.onResize_.bind(this));

    this.onResize_();
  };

  /**
   * Constructs the thumbnails grid.
   */
  WallpaperManager.prototype.initThumbnailsGrid_ = function() {
    this.wallpaperGrid_ = $('wallpaper-grid');
    wallpapers.WallpaperThumbnailsGrid.decorate(this.wallpaperGrid_);

    this.wallpaperGrid_.addEventListener('change',
        this.onThumbnailClicked_.bind(this));
    this.wallpaperGrid_.addEventListener('activate', this.onClose_.bind(this));

    var self = this;
    // Override cr.ui.List's activateItemIndex function. Called when a list item
    // is activated.
    this.wallpaperGrid_.activateItemAtIndex = this.onClose_.bind(this);
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
    if (!selectedItem || !selectedItem.dynamicURL ||
        this.wallpaperGrid_.inProgramSelection)
      return;

    // TODO(bshe): Only download one high resolution wallpaper from server.
    // Resize the high resolution wallpaper to screen sized wallpaper for
    // devices with small screen.
    var wallpaperURL = selectedItem.baseURL + HighResolutionSuffix;

    if (this.wallpaperRequest_)
      this.wallpaperRequest_.abort();
    // TODO(bshe): XMLHttpRequest may be cancelled by window.close(). We should
    // either wait for response before closing window or use an extension
    // background page to get response afte window closed.
    this.wallpaperRequest_ = new XMLHttpRequest();
    this.wallpaperRequest_.open('GET', wallpaperURL, true);
    this.wallpaperRequest_.responseType = 'arraybuffer';
    this.wallpaperRequest_.send(null);
    this.butterBar_.setRequest(this.wallpaperRequest_);
    var self = this;
    this.wallpaperRequest_.addEventListener('load', function(e) {
      if (self.wallpaperRequest_.status === 200) {
        var image = self.wallpaperRequest_.response;
        chrome.wallpaperPrivate.setWallpaper(image,
                                             selectedItem.layout,
                                             wallpaperURL);
      } else {
        // Displays the error text in butter bar.
        self.butterBar_.showError_(self.wallpaperRequest_.statusText);
      }
      self.wallpaperRequest_ = null;
    });
    this.setWallpaperAttribution_(selectedItem);
  };

  /**
   * Set attributions of wallpaper with given URL. If URL is not valid, clear
   * the attributions.
   * @param {{baseURL: string, dynamicURL: string, layout: string,
   *          author: string, authorWebsite: string}}
   *     selectedItem selected wallpaper item in grid.
   * @private
   */
  WallpaperManager.prototype.setWallpaperAttribution_ = function(selectedItem) {
    if (selectedItem) {
      $('author-name').textContent = selectedItem.author;
      $('author-website').textContent = $('author-website').href =
          selectedItem.authorWebsite;
      $('wallpaper-attribute').hidden = false;
      return;
    }
    $('wallpaper-attribute').hidden = true;
    $('author-name').textContent = '';
    $('author-website').textContent = $('author-website').href = '';
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
    var reader = new FileReader();
    reader.readAsArrayBuffer(files[0]);
    var self = this;
    // TODO(bshe): Handle file error.
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
                                               layout);
  };

  /**
   * Handles the layout setting change of custom wallpaper.
   */
  WallpaperManager.prototype.onWallpaperLayoutChanged_ = function() {
    if (this.customWallpaperData_)
      this.refreshWallpaper_(this.customWallpaperData_);
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

    if (selectedListItem.custom) {
      this.showCustomContainer_(true);
    } else {
      this.showCustomContainer_(false);
      var selectedItem;
      var wallpapersDataModel = new cr.ui.ArrayDataModel([]);
      for (var key in this.manifest_.wallpaper_list) {
        if (this.manifest_.wallpaper_list[key].categories.
                indexOf(selectedIndex) != -1) {
          var wallpaperInfo = {
            baseURL: this.manifest_.wallpaper_list[key].base_url,
            dynamicURL: this.manifest_.wallpaper_list[key].dynamic_url,
            layout: this.manifest_.wallpaper_list[key].default_layout,
            author: this.manifest_.wallpaper_list[key].author,
            authorWebsite: this.manifest_.wallpaper_list[key].author_website
          };
          wallpapersDataModel.push(wallpaperInfo);
          var url = this.manifest_.wallpaper_list[key].base_url +
              HighResolutionSuffix;
          if (url == str('selectedWallpaper')) {
            selectedItem = wallpaperInfo;
          }
        }
      }
      this.wallpaperGrid_.dataModel = wallpapersDataModel;
      this.wallpaperGrid_.selectedItem = selectedItem;
    }
  };

})();
