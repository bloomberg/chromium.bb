// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;
  var UserImagesGrid = options.UserImagesGrid;

  /** @const */ var CUSTOM_WALLPAPER_PREFIX = 'chrome://wallpaper/custom_';

  /////////////////////////////////////////////////////////////////////////////
  // SetWallpaperOptions class:

  /**
   * Encapsulated handling of ChromeOS set wallpaper options page.
   * @constructor
   */
  function SetWallpaperOptions() {
    OptionsPage.call(
        this,
        'setWallpaper',
        loadTimeData.getString('setWallpaper'),
        'set-wallpaper-page');
  }

  cr.addSingletonGetter(SetWallpaperOptions);

  SetWallpaperOptions.prototype = {
    // Inherit SetWallpaperOptions from OptionsPage.
    __proto__: options.OptionsPage.prototype,

    /**
     * Initializes SetWallpaperOptions page.
     */
    initializePage: function() {
      // Call base class implementation to start preferences initialization.
      OptionsPage.prototype.initializePage.call(this);

      var wallpaperGrid = $('wallpaper-grid');
      UserImagesGrid.decorate(wallpaperGrid);

      wallpaperGrid.addEventListener('change',
                                     this.handleImageSelected_.bind(this));
      wallpaperGrid.addEventListener('dblclick',
                                     this.handleImageDblClick_.bind(this));
      wallpaperGrid.addEventListener('activate',
                                     function() { OptionsPage.closeOverlay() });

      $('set-wallpaper-layout').addEventListener('change',
          this.handleLayoutChange_);
      $('set-custom-wallpaper').onclick = this.handleChooseFile_;
      $('use-random-wallpaper').onclick = this.handleCheckboxClick_.bind(this);
      $('set-wallpaper-overlay-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };

      // @type {Array.<author: string, url: string, website: string>}
      this.wallpapers_ = [];

      // @type {Object} Old user custom wallpaper thumbnail.
      this.oldImage_ = null;

      chrome.send('onSetWallpaperPageInitialized');
    },

    /**
     * Called right after the page has been shown to user.
     */
    didShowPage: function() {
      $('wallpaper-grid').updateAndFocus();
      // A quick hack to fix issue 118472. This is a general problem of list
      // control and options overlay.
      // TODO(bshe): Remove this hack when we fixed the general problem which
      // tracked in issue 118829.
      $('wallpaper-grid').redraw();
      chrome.send('onSetWallpaperPageShown');
    },

    /**
     * Called right before the page is hidden.
     */
    willHidePage: function() {
      var wallpaperGrid = $('wallpaper-grid');
      wallpaperGrid.blur();
      if (this.oldImage_) {
        wallpaperGrid.removeItem(this.oldImage_);
        this.oldImage_ = null;
      }
      $('set-wallpaper-layout').innerText = '';
    },

    /**
     * Set attributions of wallpaper with given URL.
     * @param {string} url URL of the selected wallpaper.
     * @private
     */
    setWallpaperAttribution_: function(url) {
      for (var i = 0; i < this.wallpapers_.length; i++) {
        if (this.wallpapers_[i].url == url) {
          $('author-name').textContent = this.wallpapers_[i].author;
          $('author-website').textContent = this.wallpapers_[i].website;
          return;
        }
      }
      $('author-name').textContent = '';
      $('author-website').textContent = '';
    },

    /**
     * Populates the drop down box for custom wallpaper layouts.
     * param {string} layouts Available wallpaper layouts.
     * param {number} selectedLayout The value of selected/default layout.
     * @private
     */
    populateWallpaperLayouts_: function(layouts, selectedLayout) {
      var wallpaperLayout = $('set-wallpaper-layout');
      var selectedIndex = -1;
      for (var i = 0; i < layouts.length; i++) {
        var option = new Option(layouts[i]['name'], layouts[i]['index']);
        if (selectedLayout == option.value)
          selectedIndex = i;
        wallpaperLayout.appendChild(option);
      }
      if (selectedIndex >= 0)
        wallpaperLayout.selectedIndex = selectedIndex;
    },

    /**
     * Handles "Custom..." button activation.
     * @private
     */
    handleChooseFile_: function() {
      chrome.send('chooseWallpaper');
    },

    /**
     * Handle the wallpaper layout setting change.
     * @private
     */
    handleLayoutChange_: function() {
      var setWallpaperLayout = $('set-wallpaper-layout');
      var layout = setWallpaperLayout.options[
          setWallpaperLayout.selectedIndex].value;
      chrome.send('changeWallpaperLayout', [layout]);
    },

    /**
     * Handles image selection change.
     * @private
     */
    handleImageSelected_: function() {
      var wallpaperGrid = $('wallpaper-grid');
      var url = wallpaperGrid.selectedItemUrl;
      if (url &&
          !wallpaperGrid.inProgramSelection) {
        if (url.indexOf(CUSTOM_WALLPAPER_PREFIX) == 0) {
          // User custom wallpaper is selected
          this.isCustom = true;
          // When users select the custom wallpaper thumbnail from picker UI,
          // use the saved layout value and redraw the wallpaper.
          this.handleLayoutChange_();
        } else {
          this.isCustom = false;
          chrome.send('selectDefaultWallpaper', [url]);
        }
        this.setWallpaperAttribution_(url);
      }
    },

    /**
     * Handles double click on the image grid.
     * @param {Event} e Double click Event.
     */
    handleImageDblClick_: function(e) {
      var wallpaperGrid = $('wallpaper-grid');
      if (wallpaperGrid.disabled)
        return;
      // Close page unless the click target is the grid itself.
      if (e.target instanceof HTMLImageElement)
        OptionsPage.closeOverlay();
    },

    /**
     * Handles click on the "I'm feeling lucky" checkbox.
     * @private
     */
    handleCheckboxClick_: function() {
      var wallpaperGrid = $('wallpaper-grid');
      if ($('use-random-wallpaper').checked) {
        wallpaperGrid.disabled = true;
        $('attribution-label').hidden = false;
        chrome.send('selectRandomWallpaper');
        wallpaperGrid.classList.add('grayout');
        $('set-wallpaper-layout').hidden = true;
      } else {
        wallpaperGrid.disabled = false;
        wallpaperGrid.classList.remove('grayout');
        // Set the wallpaper type to User::DEFAULT.
        this.handleImageSelected_();
      }
    },

    /**
     * Selects corresponding wallpaper thumbnail with the given URL and toggle
     * the "I'm feeling lucky" checkbox.
     * @param {string} url URL of the wallpaper thumbnail to select.
     * @param {boolean} isRandom True if user checked "I'm feeling lucky"
     * checkbox.
     * @private
     */
    setSelectedImage_: function(url, isRandom) {
      var wallpaperGrid = $('wallpaper-grid');
      wallpaperGrid.selectedItemUrl = url;
      this.setWallpaperAttribution_(url);
      if (isRandom) {
        // Do not call chrome.send('selectRandomWallpaper'), it is not
        // neccessary to generate a new random index here.
        $('use-random-wallpaper').checked = true;
        wallpaperGrid.disabled = true;
        wallpaperGrid.classList.add('grayout');
      }
    },

    /**
     * Appends default images to the image grid. Should only be called once.
     * @param {Array.<{author: string, url: string, website: string}>}
     * wallpapers An array of wallpaper objects.
     * @private
     */
    setDefaultImages_: function(wallpapers) {
      var wallpaperGrid = $('wallpaper-grid');
      // TODO(bshe): Ideally we should save author and website with the actual
      // image (URL) and not use index related storage. This way this data is
      // stored in one place rather than depending on the index to be
      // consistent.
      for (var i = 0, wallpaper; wallpaper = wallpapers[i]; i++) {
        this.wallpapers_.push(wallpaper);
        wallpaperGrid.addItem(wallpaper.url);
      }
    },

    /**
     * Display layout drop down box and disable random mode if enabled. Called
     * when user select a valid file from file system.
     */
    didSelectFile_: function() {
      $('set-wallpaper-layout').hidden = false;
      var wallpaperGrid = $('wallpaper-grid');
      if ($('use-random-wallpaper').checked) {
        $('use-random-wallpaper').checked = false;
        wallpaperGrid.disabled = false;
        wallpaperGrid.classList.remove('grayout');
      }
    },

    /**
     * Returns url of current user's custom wallpaper thumbnail.
     * @private
     */
    currentWallpaperImageUrl_: function() {
      return CUSTOM_WALLPAPER_PREFIX + BrowserOptions.getLoggedInUsername() +
          '?id=' + (new Date()).getTime();
    },

    /**
     * Updates the visibility of attribution-label and set-wallpaper-layout.
     * @param {boolean} isCustom True if users select custom wallpaper.
     */
    set isCustom(isCustom) {
      if (isCustom) {
        // Clear attributions for custom wallpaper.
        $('attribution-label').hidden = true;
        // Enable the layout drop down box when custom wallpaper is selected.
        $('set-wallpaper-layout').hidden = false;
      } else {
        $('attribution-label').hidden = false;
        $('set-wallpaper-layout').hidden = true;
      }
    },

    /**
     * Adds or updates custom user wallpaper thumbnail from file.
     * @private
     */
    setCustomImage_: function() {
      var wallpaperGrid = $('wallpaper-grid');
      var url = this.currentWallpaperImageUrl_();
      if (this.oldImage_) {
        this.oldImage_ = wallpaperGrid.updateItem(this.oldImage_, url);
      } else {
        // Insert to the end of wallpaper list.
        var pos = wallpaperGrid.length;
        this.oldImage_ = wallpaperGrid.addItem(url, undefined, undefined, pos);
      }

      this.isCustom = true;
      this.setWallpaperAttribution_('');
      wallpaperGrid.selectedItem = this.oldImage_;
     },
  };

  // Forward public APIs to private implementations.
  [
    'setDefaultImages',
    'setSelectedImage',
    'populateWallpaperLayouts',
    'didSelectFile',
    'setCustomImage'
  ].forEach(function(name) {
    SetWallpaperOptions[name] = function() {
      var instance = SetWallpaperOptions.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    SetWallpaperOptions: SetWallpaperOptions
  };

});

