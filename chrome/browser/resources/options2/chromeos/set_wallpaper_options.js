// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;
  var UserImagesGrid = options.UserImagesGrid;

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
        localStrings.getString('setWallpaper'),
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

      $('use-random-wallpaper').onclick = this.handleCheckboxClick_.bind(this);
      $('set-wallpaper-overlay-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };

      // @type {Array.<author: string, url: string, website: string>}
      this.wallpapers_ = [];

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
     * Handles image selection change.
     * @private
     */
    handleImageSelected_: function() {
      var wallpaperGrid = $('wallpaper-grid');
      var url = wallpaperGrid.selectedItemUrl;
      if (url &&
          !wallpaperGrid.inProgramSelection) {
        this.setWallpaperAttribution_(url);
        chrome.send('selectDefaultWallpaper', [url]);
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
        chrome.send('selectRandomWallpaper');
        wallpaperGrid.classList.add('grayout');
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

  };

  // Forward public APIs to private implementations.
  [
    'setDefaultImages',
    'setSelectedImage'
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

