// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {
  /** @const */ var ArrayDataModel = cr.ui.ArrayDataModel;
  /** @const */ var OptionsPage = options.OptionsPage;

  /**
   * This class is an overlay which allows the user to add or remove media
   * galleries, and displays known media galleries.
   * @constructor
   * @extends {OptionsPage}
   */
  function MediaGalleriesManager() {
    OptionsPage.call(this, 'manageGalleries',
                     loadTimeData.getString('manageMediaGalleriesTabTitle'),
                     'manage-media-galleries-overlay');
  }

  cr.addSingletonGetter(MediaGalleriesManager);

  MediaGalleriesManager.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Decorate the overlay and set up event handlers.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      this.availableGalleriesList_ = $('available-galleries-list');
      options.MediaGalleriesList.decorate(this.availableGalleriesList_);

      $('new-media-gallery').addEventListener('click', function() {
        chrome.send('addNewGallery');
      });

      $('manage-media-confirm').addEventListener(
          'click', OptionsPage.closeOverlay.bind(OptionsPage));

      this.addEventListener('visibleChange', this.handleVisibleChange_);
    },

    /**
     * TODO(dbeam): why is a private method being overridden?
     * @override
     * @private
     */
    handleVisibleChange_: function() {
      if (!this.visible)
        return;

      if (this.availableGalleriesList_)
        this.availableGalleriesList_.redraw();
    },

    /**
     * @param {Array} galleries List of structs describibing galleries.
     * @private
     */
    setAvailableMediaGalleries_: function(galleries) {
      $('available-galleries-list').dataModel = new ArrayDataModel(galleries);
      // TODO(estade): show this section by default.
      $('media-galleries-section').hidden = false;
    },
  },

  // Forward public APIs to private implementations.
  [
    'setAvailableMediaGalleries',
  ].forEach(function(name) {
    MediaGalleriesManager[name] = function() {
      var instance = MediaGalleriesManager.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    MediaGalleriesManager: MediaGalleriesManager
  };
});
