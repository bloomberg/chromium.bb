// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


'use strict';

let customBackgrounds = {};

/**
 * Enum for key codes.
 * @enum {int}
 * @const
 */
customBackgrounds.KEYCODES = {
  BACKSPACE: 8,
  ENTER: 13,
  ESC: 27,
  TAB: 9,
};

/**
 * Enum for HTML element ids.
 * @enum {string}
 * @const
 */
customBackgrounds.IDS = {
  BACK: 'bg-sel-back',
  CANCEL: 'bg-sel-footer-cancel',
  CONNECT_GOOGLE_PHOTOS: 'edit-bg-google-photos',
  CONNECT_GOOGLE_PHOTOS_TEXT: 'edit-bg-google-photos-text',
  DEFAULT_WALLPAPERS: 'edit-bg-default-wallpapers',
  DEFAULT_WALLPAPERS_TEXT: 'edit-bg-default-wallpapers-text',
  DONE: 'bg-sel-footer-done',
  EDIT_BG: 'edit-bg',
  EDIT_BG_DIALOG: 'edit-bg-dialog',
  MENU: 'bg-sel-menu',
  OPTIONS_TITLE: 'edit-bg-title',
  RESTORE_DEFAULT: 'edit-bg-restore-default',
  RESTORE_DEFAULT_TEXT: 'edit-bg-restore-default-text',
  REFRESH_TEXT: 'bg-sel-refresh-text',
  REFRESH_TOGGLE: 'bg-daily-refresh',
  UPLOAD_IMAGE: 'edit-bg-upload-image',
  UPLOAD_IMAGE_TEXT: 'edit-bg-upload-image-text',
  TILES: 'bg-sel-tiles',
  TITLE: 'bg-sel-title',
};

/**
 * Enum for classnames.
 * @enum {string}
 * @const
 */
customBackgrounds.CLASSES = {
  COLLECTION_DIALOG: 'is-col-sel',
  COLLECTION_SELECTED: 'bg-selected',  // Highlight selected tile
  COLLECTION_TILE: 'bg-sel-tile',  // Preview tile for background customization
  COLLECTION_TITLE: 'bg-sel-tile-title',  // Title of a background image
  DONE_AVAILABLE: 'done-available',
  IMAGE_DIALOG: 'is-img-sel',
  KEYBOARD_NAV: 'using-keyboard-nav',
  SELECTED_BORDER: 'selected-border',
  SELECTED_CHECK: 'selected-check',
  SELECTED_CIRCLE: 'selected-circle',
};

/**
 * Enum for background sources.
 * @enum {int}
 * @const
 */
customBackgrounds.SOURCES = {
  NONE: -1,
  CHROME_BACKGROUNDS: 0,
  GOOGLE_PHOTOS: 1,
  IMAGE_UPLOAD: 2,
};

customBackgrounds.CUSTOM_BACKGROUND_OVERLAY =
    'linear-gradient(rgba(0, 0, 0, 0.2), rgba(0, 0, 0, 0.2))';

/* Tile that was selected by the user.
 * @type {HTMLElement}
 */
customBackgrounds.selectedTile = null;

/* Type of collection that is being browsed, needed in order
 * to return from the image dialog.
 * @type {int}
 */
customBackgrounds.dialogCollectionsSource = customBackgrounds.SOURCES.NONE;

/**
 * Alias for document.getElementById.
 * @param {string} id The ID of the element to find.
 * @return {HTMLElement} The found element or null if not found.
 */
function $(id) {
  // eslint-disable-next-line no-restricted-properties
  return document.getElementById(id);
}

/**
 * Remove all collection tiles from the container when the dialog
 * is closed.
 */
customBackgrounds.resetSelectionDialog = function() {
  $(customBackgrounds.IDS.TILES).scrollTop = 0;
  var tileContainer = $(customBackgrounds.IDS.TILES);
  while (tileContainer.firstChild) {
    tileContainer.removeChild(tileContainer.firstChild);
  }
  $(customBackgrounds.IDS.DONE)
      .classList.remove(customBackgrounds.CLASSES.DONE_AVAILABLE);
  customBackgrounds.selectedTile = null;
};

/* Close the collection selection dailog and cleanup the state
 * @param {dialog} menu The dialog to be closed
 */
customBackgrounds.closeCollectionDialog = function(menu) {
  menu.close();
  customBackgrounds.dialogCollectionsSource = customBackgrounds.SOURCES.NONE;
  customBackgrounds.resetSelectionDialog();
};

/* Close and reset the dialog, and set the background.
 * @param {string} url The url of the selected background.
 */
customBackgrounds.setBackground = function(url) {
  customBackgrounds.closeCollectionDialog($(customBackgrounds.IDS.MENU));
  window.chrome.embeddedSearch.newTabPage.setBackgroundURL(url);
};

/**
 * Create a tile for a Chrome Backgrounds collection.
 */
customBackgrounds.createChromeBackgroundTile = function(data) {
  var tile = document.createElement('div');
  tile.style.backgroundImage = 'url(' + data.previewImageUrl + ')';
  tile.dataset.id = data.collectionId;
  tile.dataset.name = data.collectionName;
  return tile;
};

/**
 * Create a tile for a Google Photos album.
 */
customBackgrounds.createAlbumTile = function(data) {
  var tile = document.createElement('div');
  tile.style.backgroundImage = 'url(' + data.previewImageUrl + ')';
  tile.dataset.id = data.albumId;
  tile.dataset.name = data.albumName;
  tile.dataset.photoContainerId = data.photoContainerId;
  return tile;
};

/**
 * Show dialog for selecting either a Chrome background collection or Google
 * Photo album. Draw data from either coll or albums.
 * @param {int} collectionsSource The enum value of the source to fetch
 *              collection data from.
 */
customBackgrounds.showCollectionSelectionDialog = function(collectionsSource) {
  var tileContainer = $(customBackgrounds.IDS.TILES);
  var menu = $(customBackgrounds.IDS.MENU);
  var collData = null;
  var sourceIsChromeBackgrounds =
      (collectionsSource == customBackgrounds.SOURCES.CHROME_BACKGROUNDS);
  if (collectionsSource != customBackgrounds.SOURCES.CHROME_BACKGROUNDS &&
      collectionsSource != customBackgrounds.SOURCES.GOOGLE_PHOTOS) {
    console.log(
        'showCollectionSelectionDialog() called with invalid source=' +
        collectionsSource);
    return;
  }
  customBackgrounds.dialogCollectionsSource = collectionsSource;

  if (!menu.open)
    menu.showModal();

  // Create dialog header.
  if (sourceIsChromeBackgrounds) {
    $(customBackgrounds.IDS.TITLE).textContent =
        configData.translatedStrings.selectChromeWallpaper;
    collData = coll;
  } else {
    $(customBackgrounds.IDS.TITLE).textContent =
        configData.translatedStrings.selectGooglePhotoAlbum;
    collData = albums;
  }
  menu.classList.add(customBackgrounds.CLASSES.COLLECTION_DIALOG);
  menu.classList.remove(customBackgrounds.CLASSES.IMAGE_DIALOG);

  // Create dialog tiles.
  for (var i = 0; i < collData.length; ++i) {
    var tile = null;
    if (sourceIsChromeBackgrounds) {
      tile = customBackgrounds.createChromeBackgroundTile(collData[i]);
    } else {
      tile = customBackgrounds.createAlbumTile(collData[i]);
    }
    tile.classList.add(customBackgrounds.CLASSES.COLLECTION_TILE);
    tile.id = 'coll_tile_' + i;
    tile.tabIndex = 0;

    var title = document.createElement('div');
    title.classList.add(customBackgrounds.CLASSES.COLLECTION_TITLE);
    title.textContent = tile.dataset.name;

    var tileInteraction = function(event) {
      var tile = event.target;
      if (tile.classList.contains(customBackgrounds.CLASSES.COLLECTION_TITLE))
        tile = tile.parentNode;

      // Load images for selected collection.
      var imgElement = $('ntp-images-loader');
      if (imgElement) {
        imgElement.parentNode.removeChild(imgElement);
      }
      var imgScript = document.createElement('script');
      imgScript.id = 'ntp-images-loader';

      if (sourceIsChromeBackgrounds) {
        imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
            'collection_type=background&collection_id=' + tile.dataset.id;
      } else {
        imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
            'collection_type=album&album_id=' + tile.dataset.id +
            '&photo_container_id=' + tile.dataset.photoContainerId;
      }

      document.body.appendChild(imgScript);

      imgScript.onload = function() {
        // Verify that the individual image data was successfully loaded.
        var imageDataLoaded = false;
        if (sourceIsChromeBackgrounds) {
          imageDataLoaded =
              (coll_img.length > 0 &&
               coll_img[0].collectionId == tile.dataset.id);
        } else {
          imageDataLoaded =
              (photos.length > 0 && photos[0].albumId == tile.dataset.id &&
               photos[0].photoContainerId == tile.dataset.photoContainerId);
        }

        // Dependent upon the succces of the load, populate the image selection
        // dialog or close the current dialog.
        customBackgrounds.resetSelectionDialog();
        if (imageDataLoaded) {
          customBackgrounds.showImageSelectionDialog(tile.dataset.name);
        } else {
          customBackgrounds.closeCollectionDialog(menu);
        }
      };
    };

    tile.onclick = tileInteraction;
    tile.onkeyup = function(event) {
      if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
        tileInteraction(event);
      }
    };

    tile.appendChild(title);
    tileContainer.appendChild(tile);
  }

  $(customBackgrounds.IDS.DONE).tabIndex = -1;
  $('coll_tile_0').focus();
};

/**
 * Apply border and checkmark when a tile is selected
 * @param {div} tile The tile to apply styling to.
 */
customBackgrounds.applySelectedState = function(tile) {
  tile.classList.add(customBackgrounds.CLASSES.COLLECTION_SELECTED);
  var selectedBorder = document.createElement('div');
  var selectedCircle = document.createElement('div');
  var selectedCheck = document.createElement('div');
  selectedBorder.classList.add(customBackgrounds.CLASSES.SELECTED_BORDER);
  selectedCircle.classList.add(customBackgrounds.CLASSES.SELECTED_CIRCLE);
  selectedCheck.classList.add(customBackgrounds.CLASSES.SELECTED_CHECK);
  selectedBorder.appendChild(selectedCircle);
  selectedBorder.appendChild(selectedCheck);
  tile.appendChild(selectedBorder);
};

/**
 * Remove border and checkmark when a tile is un-selected
 * @param {div} tile The tile to remove styling from.
 */
customBackgrounds.removeSelectedState = function(tile) {
  tile.classList.remove(customBackgrounds.CLASSES.COLLECTION_SELECTED);
  tile.removeChild(tile.firstChild);
};

/**
 * Show dialog for selecting an image or toggling on daily refresh. Image
 * data should previous have been loaded into coll_img via
 * chrome-search://local-ntp/ntp-background-images.js?collection_id=<collection_id>
 * @param {string} dialogTitle The title to be displayed at the top of the
 *                 dialog.
 */
customBackgrounds.showImageSelectionDialog = function(dialogTitle) {
  var menu = $(customBackgrounds.IDS.MENU);
  var tileContainer = $(customBackgrounds.IDS.TILES);
  var sourceIsChromeBackgrounds =
      (customBackgrounds.dialogCollectionsSource ==
       customBackgrounds.SOURCES.CHROME_BACKGROUNDS);

  $(customBackgrounds.IDS.TITLE).textContent = dialogTitle;
  menu.classList.remove(customBackgrounds.CLASSES.COLLECTION_DIALOG);
  menu.classList.add(customBackgrounds.CLASSES.IMAGE_DIALOG);

  var imageData = null;
  if (sourceIsChromeBackgrounds) {
    imageData = coll_img;
  } else {
    imageData = photos;
  }

  for (var i = 0; i < imageData.length; ++i) {
    var tile = document.createElement('div');
    tile.classList.add(customBackgrounds.CLASSES.COLLECTION_TILE);

    // Set the background image, the name of the source variable differs
    // depending on if it's coming from Chrome Backgrounds or Google Photos.
    if (sourceIsChromeBackgrounds) {
      // TODO(crbug.com/854028): Remove this hardcoded check when wallpaper
      // previews are supported.
      if (imageData[i].collectionId == 'solidcolors') {
        var imageWithOverlay = [
          customBackgrounds.CUSTOM_BACKGROUND_OVERLAY,
          'url(' + imageData[i].thumbnailImageUrl + ')'
        ].join(',').trim();
        tile.style.backgroundImage = imageWithOverlay;
      } else {
        tile.style.backgroundImage =
            'url(' + imageData[i].thumbnailImageUrl + ')';
      }
      tile.dataset.url = imageData[i].imageUrl;
    } else {
      tile.style.backgroundImage =
          'url(' + imageData[i].thumbnailPhotoUrl + ')';
      tile.dataset.url = imageData[i].photoUrl;
    }

    tile.id = 'img_tile_' + i;
    tile.tabIndex = 0;

    var tileInteraction = function(event) {
      var tile = event.target;
      if (customBackgrounds.selectedTile) {
        customBackgrounds.removeSelectedState(customBackgrounds.selectedTile);
      }
      customBackgrounds.selectedTile = tile;

      customBackgrounds.applySelectedState(tile);

      // Turn toggle off when an image is selected.
      $(customBackgrounds.IDS.REFRESH_TOGGLE).children[0].checked = false;
      $(customBackgrounds.IDS.DONE)
          .classList.add(customBackgrounds.CLASSES.DONE_AVAILABLE);
    };

    tile.onclick = function(event) {
      let clickCount = event.detail;
      if (clickCount == 1) {
        tileInteraction(event);
      } else if (clickCount == 2) {
        customBackgrounds.setBackground(this.dataset.url);
      }
    };
    tile.onkeyup = function(event) {
      if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
        tileInteraction(event);
        doneButton.focus();
      }
    };

    tileContainer.appendChild(tile);
  }

  $(customBackgrounds.IDS.DONE).tabIndex = 0;
  $('img_tile_0').focus();
};

/**
 * Load the NTPBackgroundCollections script. It'll create a global
 * variable name "coll" which is a dict of background collections data.
 * TODO(kmilka): add error UI as part of crbug.com/848981.
 * @private
 */
customBackgrounds.loadCollections = function() {
  var collElement = $('ntp-collection-loader');
  if (collElement) {
    collElement.parentNode.removeChild(collElement);
  }
  var collScript = document.createElement('script');
  collScript.id = 'ntp-collection-loader';
  collScript.src = 'chrome-search://local-ntp/ntp-background-collections.js?' +
      'collection_type=background';
  document.body.appendChild(collScript);

  var albumElement = $('ntp-album-loader');
  if (albumElement) {
    albumElement.parentNode.removeChild(albumElement);
  }
  var albumScript = document.createElement('script');
  albumScript.id = 'ntp-album-loader';
  albumScript.src = 'chrome-search://local-ntp/ntp-background-collections.js?' +
      'collection_type=album';
  document.body.appendChild(albumScript);
};

/* Close dialog when an image is selected via the file picker. */
customBackgrounds.closeCustomizationDialog = function() {
  $(customBackgrounds.IDS.EDIT_BG_DIALOG).close();
  $(customBackgrounds.IDS.EDIT_BG).focus();
};

/**
 * Initialize the custom backgrounds dialogs. Set the text and event handlers
 * for the various elements.
 */
customBackgrounds.initCustomBackgrounds = function() {
  var editDialog = $(customBackgrounds.IDS.EDIT_BG_DIALOG);
  var menu = $(customBackgrounds.IDS.MENU);

  $(customBackgrounds.IDS.CONNECT_GOOGLE_PHOTOS_TEXT).textContent =
      configData.translatedStrings.connectGooglePhotos;
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS_TEXT).textContent =
      configData.translatedStrings.defaultWallpapers;
  $(customBackgrounds.IDS.UPLOAD_IMAGE_TEXT).textContent =
      configData.translatedStrings.uploadImage;
  $(customBackgrounds.IDS.RESTORE_DEFAULT_TEXT).textContent =
      configData.translatedStrings.restoreDefaultBackground;
  $(customBackgrounds.IDS.OPTIONS_TITLE).textContent =
      configData.translatedStrings.customizeBackground;
  $(customBackgrounds.IDS.REFRESH_TEXT).textContent =
      configData.translatedStrings.dailyRefresh;
  $(customBackgrounds.IDS.DONE).textContent =
      configData.translatedStrings.selectionDone;
  $(customBackgrounds.IDS.CANCEL).textContent =
      configData.translatedStrings.selectionCancel;

  // TODO(kmilka): files should be validated and have errors shown as needed.
  // crbug.com/848981.
  var uploadImageInteraction = function(event) {
    window.chrome.embeddedSearch.newTabPage.selectLocalBackgroundImage();
  };

  $(customBackgrounds.IDS.UPLOAD_IMAGE).onclick = uploadImageInteraction;
  $(customBackgrounds.IDS.UPLOAD_IMAGE).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      uploadImageInteraction(event);
    }
  };

  // Edit gear icon interaction events.
  var editBackgroundInteraction = function(event) {
    customBackgrounds.loadCollections();
    editDialog.showModal();
  };
  $(customBackgrounds.IDS.EDIT_BG).onclick = function(event) {
    editDialog.classList.remove(customBackgrounds.CLASSES.KEYBOARD_NAV);
    editBackgroundInteraction(event);
  };
  $(customBackgrounds.IDS.EDIT_BG).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      editDialog.classList.add(customBackgrounds.CLASSES.KEYBOARD_NAV);
      editBackgroundInteraction(event);
    }
  };

  // Interactions to close the customization option dialog.
  var editDialogInteraction = function(event) {
    editDialog.close();
  };
  editDialog.onclick = function(event) {
    if (event.target == editDialog)
      editDialogInteraction(event);
  };
  editDialog.onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ESC ||
        event.keyCode === customBackgrounds.KEYCODES.BACKSPACE) {
      editDialogInteraction(event);
    }
  };

  // Interactions with the "Restore default background" option.
  var restoreDefaultInteraction = function(event) {
    editDialog.close();
    window.chrome.embeddedSearch.newTabPage.setBackgroundURL('');
  };
  $(customBackgrounds.IDS.RESTORE_DEFAULT).onclick = restoreDefaultInteraction;
  $(customBackgrounds.IDS.RESTORE_DEFAULT).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      restoreDefaultInteraction(event);
    }
  };

  // Interactions with the "Chrome backgrounds" option.
  var defaultWallpapersInteraction = function(event) {
    editDialog.close();
    if (typeof coll != 'undefined' && coll.length > 0) {
      customBackgrounds.showCollectionSelectionDialog(
          customBackgrounds.SOURCES.CHROME_BACKGROUNDS);
    }
  };
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).onclick =
      defaultWallpapersInteraction;
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      defaultWallpapersInteraction(event);
    }
  };

  // Interactions with the Google Photos option.
  var googlePhotosInteraction = function(event) {
    editDialog.close();
    if (typeof albums != 'undefined' && albums.length > 0) {
      customBackgrounds.showCollectionSelectionDialog(
          customBackgrounds.SOURCES.GOOGLE_PHOTOS);
    }
  };
  $(customBackgrounds.IDS.CONNECT_GOOGLE_PHOTOS).onclick =
      googlePhotosInteraction;
  $(customBackgrounds.IDS.CONNECT_GOOGLE_PHOTOS).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      googlePhotosInteraction(event);
    }
  };

  // Escape and Backspace handling for the background picker dialog.
  menu.onkeydown = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ESC ||
        event.keyCode === customBackgrounds.KEYCODES.BACKSPACE) {
      event.preventDefault();
      event.stopPropagation();
      if (menu.classList.contains(
              customBackgrounds.CLASSES.COLLECTION_DIALOG)) {
        menu.close();
        customBackgrounds.resetSelectionDialog();
      } else {
        customBackgrounds.resetSelectionDialog();
        customBackgrounds.showCollectionSelectionDialog(
            customBackgrounds.dialogCollectionsSource);
      }
    }
  };

  // Clicks that happen outside of the dialog.
  menu.onclick = function(event) {
    if (event.target == menu) {
      customBackgrounds.closeCollectionDialog(menu);
    }
  };

  // Interactions with the back arrow on the image selection dialog.
  var backInteraction = function(event) {
    customBackgrounds.resetSelectionDialog();
    customBackgrounds.showCollectionSelectionDialog(
        customBackgrounds.dialogCollectionsSource);
  };
  $(customBackgrounds.IDS.BACK).onclick = backInteraction;
  $(customBackgrounds.IDS.BACK).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      backInteraction(event);
    }
  };

  // Interactions with the cancel button on the background picker dialog.
  $(customBackgrounds.IDS.CANCEL).onclick = function(event) {
    customBackgrounds.closeCollectionDialog(menu);
  };
  $(customBackgrounds.IDS.CANCEL).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      customBackgrounds.closeCollectionDialog(menu);
    }
  };

  // Interactions with the done button on the background picker dialog.
  var doneInteraction = function(event) {
    if (!$(customBackgrounds.IDS.DONE)
             .classList.contains(customBackgrounds.CLASSES.DONE_AVAILABLE)) {
      return;
    }

    customBackgrounds.setBackground(customBackgrounds.selectedTile.dataset.url);
  };
  $(customBackgrounds.IDS.DONE).onclick = doneInteraction;
  $(customBackgrounds.IDS.DONE).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      doneInteraction(event);
    }
  };

  // Interactions with the "Daily refresh" toggle.
  $(customBackgrounds.IDS.REFRESH_TOGGLE).onclick = function(event) {
    if (customBackgrounds.selectedTile) {
      customBackgrounds.removeSelectedState(customBackgrounds.selectedTile);
      customBackgrounds.selectedTile = null;
    }
    $(customBackgrounds.IDS.DONE)
        .classList.add(customBackgrounds.CLASSES.DONE_AVAILABLE);
  };
};
