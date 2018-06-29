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
};

customBackgrounds.CUSTOM_BACKGROUND_OVERLAY =
    'linear-gradient(rgba(0, 0, 0, 0.2), rgba(0, 0, 0, 0.2))';

/* Tile that was selected by the user.
 * @type {HTMLElement}
 */
customBackgrounds.selectedTile = null;

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
  customBackgrounds.resetSelectionDialog();
};

/**
 * Show dialog for selecting a collection. Populates the dialog
 * with data from |coll|.
 * @param {string} dialogTitle The title to be displayed at the top of the
 * dialog.
 */
customBackgrounds.showCollectionSelectionDialog = function() {
  var tileContainer = $(customBackgrounds.IDS.TILES);
  var menu = $(customBackgrounds.IDS.MENU);

  if (!menu.open)
    menu.showModal();

  // Create dialog header
  $(customBackgrounds.IDS.TITLE).textContent =
      configData.translatedStrings.selectChromeWallpaper;
  menu.classList.add(customBackgrounds.CLASSES.COLLECTION_DIALOG);
  menu.classList.remove(customBackgrounds.CLASSES.IMAGE_DIALOG);

  // Create dialog tiles
  for (var i = 0; i < coll.length; ++i) {
    var tile = document.createElement('div');
    tile.classList.add(customBackgrounds.CLASSES.COLLECTION_TILE);
    tile.style.backgroundImage = 'url(' + coll[i].previewImageUrl + ')';
    tile.id = 'coll_tile_' + i;
    tile.dataset.id = coll[i].collectionId;
    tile.dataset.name = coll[i].collectionName;
    tile.tabIndex = 0;

    var title = document.createElement('div');
    title.classList.add(customBackgrounds.CLASSES.COLLECTION_TITLE);
    title.textContent = tile.dataset.name;

    var tileInteraction = function(event) {
      var tile = event.target;
      if (tile.classList.contains(customBackgrounds.CLASSES.COLLECTION_TITLE))
        tile = tile.parentNode;

      // Load images for selected collection
      var imgElement = $('ntp-images-loader');
      if (imgElement) {
        imgElement.parentNode.removeChild(imgElement);
      }
      var imgScript = document.createElement('script');
      imgScript.id = 'ntp-images-loader';
      imgScript.src = 'chrome-search://local-ntp/ntp-background-images.js?' +
          'collection_id=' + tile.dataset.id;
      document.body.appendChild(imgScript);

      imgScript.onload = function() {
        customBackgrounds.resetSelectionDialog();
        if (coll_img.length > 0 &&
            coll_img[0].collectionId == tile.dataset.id) {
          customBackgrounds.showImageSelectionDialog(tile.dataset.name);
        } else {
          customBackgrounds.closeCollectionDialog();
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
  var selectedCheck = document.createElement('div');
  selectedBorder.classList.add(customBackgrounds.CLASSES.SELECTED_BORDER);
  selectedCheck.classList.add(customBackgrounds.CLASSES.SELECTED_CHECK);
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
 */
customBackgrounds.showImageSelectionDialog = function(dialogTitle) {
  var menu = $(customBackgrounds.IDS.MENU);
  var tileContainer = $(customBackgrounds.IDS.TILES);

  $(customBackgrounds.IDS.TITLE).textContent = dialogTitle;
  menu.classList.remove(customBackgrounds.CLASSES.COLLECTION_DIALOG);
  menu.classList.add(customBackgrounds.CLASSES.IMAGE_DIALOG);

  for (var i = 0; i < coll_img.length; ++i) {
    var tile = document.createElement('div');
    tile.classList.add(customBackgrounds.CLASSES.COLLECTION_TILE);

    // TODO(crbug.com/854028): Remove this hardcoded check when wallpaper
    // previews are supported.
    if (coll_img[i].collectionId == 'solidcolors') {
      var imageWithOverlay = [
        customBackgrounds.CUSTOM_BACKGROUND_OVERLAY,
        'url(' + coll_img[i].thumbnailImageUrl + ')'
      ].join(',').trim();
      tile.style.backgroundImage = imageWithOverlay;
    } else {
      tile.style.backgroundImage = 'url(' + coll_img[i].thumbnailImageUrl + ')';
    }

    tile.id = 'img_tile_' + i;
    tile.dataset.url = coll_img[i].imageUrl;
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

    tile.onclick = tileInteraction;
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
  collScript.src = 'chrome-search://local-ntp/ntp-background-collections.js';
  document.body.appendChild(collScript);
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
    if (typeof coll != 'undefined') {
      customBackgrounds.showCollectionSelectionDialog();
    }
  };
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).onclick =
      defaultWallpapersInteraction;
  $(customBackgrounds.IDS.DEFAULT_WALLPAPERS).onkeyup = function(event) {
    if (event.keyCode === customBackgrounds.KEYCODES.ENTER) {
      defaultWallpapersInteraction(event);
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
        customBackgrounds.showCollectionSelectionDialog();
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
    customBackgrounds.showCollectionSelectionDialog();
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

    menu.close();
    window.chrome.embeddedSearch.newTabPage.setBackgroundURL(
        customBackgrounds.selectedTile.dataset.url);
    customBackgrounds.resetSelectionDialog();
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
