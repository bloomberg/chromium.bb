// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The current selection object.
 * @param {FileManager} fileManager FileManager instance.
 * @param {Array.<Number>} indexes Selected indexes.
 */
function Selection(fileManager, indexes) {
  this.fileManager_ = fileManager;
  this.indexes = indexes;
  this.entries = [];
  this.urls = [];
  this.totalCount = 0;
  this.fileCount = 0;
  this.directoryCount = 0;
  this.bytes = 0;
  this.showBytes = false;
  this.allGDataFilesPresent = false,
  this.iconType = null;


  // Synchronously compute what we can.
  for (var i = 0; i < this.indexes.length; i++) {
    var entry = fileManager.getFileList().item(this.indexes[i]);
    if (!entry)
      continue;

    this.entries.push(entry);
    this.urls.push(entry.toURL());

    if (this.iconType == null) {
      this.iconType = FileType.getIcon(entry);
    } else if (this.iconType != 'unknown') {
      var iconType = FileType.getIcon(entry);
      if (this.iconType != iconType)
        this.iconType = 'unknown';
    }

    if (entry.isFile) {
      this.fileCount += 1;
      this.showBytes |= !FileType.isHosted(entry);
    } else {
      this.directoryCount += 1;
    }
    this.totalCount++;
  }

  this.tasks = new FileTasks(this.fileManager_);
}

/**
 * Computes data required to get file tasks and requests the tasks.
 * @param {function} callback The callback.
 */
Selection.prototype.createTasks = function(callback) {
  if (!this.fileManager_.isOnGData()) {
    this.tasks.init(this.urls);
    callback();
    return;
  }

  this.fileManager_.metadataCache_.get(this.urls, 'gdata', function(props) {
    var present = props.filter(function(p) { return p && p.availableOffline });
    this.allGDataFilesPresent = present.length == props.length;

    // Collect all of the mime types and push that info into the selection.
    this.mimeTypes = props.map(function(value) {
      return (value && value.contentMimeType) || '';
    });

    this.tasks.init(this.urls, this.mimeTypes);
    callback();
  }.bind(this));
};

/**
 * Computes the total size of selected files.
 * @param {function} callback The callback.
 */
Selection.prototype.computeBytes = function(callback) {
  var onProps = function(props) {
    for (var index = 0; index < this.entries.length; index++) {
      var filesystem = props[index];
      if (this.entries[index].isFile) {
        this.bytes += filesystem.size;
      }
    }
    callback();
  }.bind(this);

  this.fileManager_.metadataCache_.get(this.entries, 'filesystem', onProps);
};

/**
 * This object encapsulates everything related to current selection.
 * @param {FileManager} fileManager File manager instance.
 */
function SelectionHandler(fileManager) {
  this.fileManager_ = fileManager;
  // TODO(dgozman): create a shared object with most of UI elements.
  this.okButton_ = fileManager.okButton_;
  this.filenameInput_ = fileManager.filenameInput_;
  this.previewPanel_ = fileManager.previewPanel_;
  this.previewThumbnails_ = fileManager.previewThumbnails_;
  this.previewSummary_ = fileManager.previewSummary_;
  this.searchBreadcrumbs_ = fileManager.searchBreadcrumbs_;
  this.taskItems_ = fileManager.taskItems_;
}

/**
 * Maximum amount of thumbnails in the preview pane.
 */
SelectionHandler.MAX_PREVIEW_THUMBNAIL_COUNT = 4;

/**
 * Maximum width or height of an image what pops up when the mouse hovers
 * thumbnail in the bottom panel (in pixels).
 */
SelectionHandler.IMAGE_HOVER_PREVIEW_SIZE = 200;

/**
 * Update the UI when the selection model changes.
 *
 * @param {cr.Event} event The change event.
 */
SelectionHandler.prototype.onSelectionChanged = function(event) {
  var indexes =
      this.fileManager_.getCurrentList().selectionModel.selectedIndexes;
  var selection = this.selection = new Selection(this.fileManager_, indexes);

  this.updateSelectionCheckboxes_(event);

  if (this.fileManager_.dialogType == DialogType.SELECT_SAVEAS_FILE) {
    // If this is a save-as dialog, copy the selected file into the filename
    // input text box.
    if (this.selection.totalCount == 1 &&
        this.selection.entries[0].isFile &&
        this.filenameInput_.value != this.selection.entries[0].name) {
      this.filenameInput_.value = this.selection.entries[0].name;
    }
  }

  this.updateOkButton();

  if (this.selectionUpdateTimer_) {
    clearTimeout(this.selectionUpdateTimer_);
    this.selectionUpdateTimer_ = null;

    // The selection is changing quicker than we can update the UI.
    // Clear the UI.
    this.clearUI();
  }

  if (!indexes.length) {
    this.updatePreviewPanelVisibility_();
    this.updatePreviewPanelText_();
    return;
  }

  this.previewSummary_.textContent = str('COMPUTING_SELECTION');

  // The rest of the selection properties are computed via (sometimes lengthy)
  // asynchronous calls. We initiate these calls after a timeout. If the
  // selection is changing quickly we only do this once when it slows down.

  var updateDelay = 200;
  var now = Date.now();
  if (now > (this.lastSelectionTime_ || 0) + updateDelay) {
    // The previous selection change happened a while ago. Update the UI soon.
    updateDelay = 0;
  }
  this.lastSelectionTime_ = now;

  this.selectionUpdateTimer_ = setTimeout(function() {
    this.selectionUpdateTimer_ = null;
    if (this.selection == selection)
      this.updateSelectionAsync(selection);
  }.bind(this), updateDelay);
};

/**
 * Clears the primary UI selection elements.
 */
SelectionHandler.prototype.clearUI = function() {
  this.previewThumbnails_.textContent = '';
  this.taskItems_.hidden = true;
  this.okButton_.disabled = true;
};

/**
 * Update the selection checkboxes.
 * @param {Event} event The event.
 * @private
 */
SelectionHandler.prototype.updateSelectionCheckboxes_ = function(event) {
  var fm = this.fileManager_;
  if (!fm.showCheckboxes_)
    return;

  if (event && event.changes) {
    for (var i = 0; i < event.changes.length; i++) {
      // Turn off any checkboxes for items that are no longer selected.
      var selectedIndex = event.changes[i].index;
      var listItem = fm.currentList_.getListItemByIndex(selectedIndex);
      if (!listItem) {
        // When changing directories, we get notified about list items
        // that are no longer there.
        continue;
      }

      if (!event.changes[i].selected) {
        var checkbox = listItem.querySelector('input[type="checkbox"]');
        checkbox.checked = false;
      }
    }
  } else {
    var checkboxes = fm.currentList_.querySelectorAll('input[type="checkbox"]');
    for (var i = 0; i < checkboxes.length; i++) {
      checkboxes[i].checked = false;
    }
  }

  for (var i = 0; i < this.selection.entries.length; i++) {
    var selectedIndex = this.selection.indexes[i];
    var listItem = fm.currentList_.getListItemByIndex(selectedIndex);
    if (listItem)
      listItem.querySelector('input[type="checkbox"]').checked = true;
  }

  this.updateSelectAllCheckboxState();
};

/**
 * Update check and disable states of the 'Select all' checkbox.
 * @param {HTMLInputElement=} opt_checkbox The checkbox. If not passed, using
 *     the default one.
 */
SelectionHandler.prototype.updateSelectAllCheckboxState = function(
    opt_checkbox) {
  var checkbox = opt_checkbox ||
      this.fileManager_.document_.getElementById('select-all-checkbox');
  if (!checkbox) return;

  var dm = this.fileManager_.getFileList();
  checkbox.checked = this.selection && dm.length > 0 &&
                     dm.length == this.selection.totalCount;
  checkbox.disabled = dm.length == 0;
};

/**
 * Updates the Ok button enabled state.
 * @return {boolean} Whether button is enabled.
 */
SelectionHandler.prototype.updateOkButton = function() {
  var selectable;
  var dialogType = this.fileManager_.dialogType;

  if (dialogType == DialogType.SELECT_FOLDER) {
    // In SELECT_FOLDER mode, we allow to select current directory
    // when nothing is selected.
    selectable = this.selection.directoryCount <= 1 &&
        this.selection.fileCount == 0;
  } else if (dialogType == DialogType.SELECT_OPEN_FILE) {
    selectable = (this.isSelectionAvailable() &&
                  this.selection.directoryCount == 0 &&
                  this.selection.fileCount == 1);
  } else if (dialogType == DialogType.SELECT_OPEN_MULTI_FILE) {
    selectable = (this.isSelectionAvailable() &&
                  this.selection.directoryCount == 0 &&
                  this.selection.fileCount >= 1);
  } else if (dialogType == DialogType.SELECT_SAVEAS_FILE) {
    if (this.fileManager_.isOnReadonlyDirectory()) {
      selectable = false;
    } else {
      selectable = !!this.filenameInput_.value;
    }
  } else if (dialogType == DialogType.FULL_PAGE) {
    // No "select" buttons on the full page UI.
    selectable = true;
  } else {
    throw new Error('Unknown dialog type');
  }

  this.okButton_.disabled = !selectable;
  return selectable;
};

/**
  * Check if all the files in the current selection are available. The only
  * case when files might be not available is when the selection contains
  * uncached GData files and the browser is offline.
  * @return {boolean} True if all files in the current selection are
  *                   available.
  */
SelectionHandler.prototype.isSelectionAvailable = function() {
  return !this.fileManager_.isOnGData() ||
      !this.fileManager_.isOffline() ||
      this.selection.allGDataFilesPresent;
};

/**
 * Animates preview panel show/hide transitions.
 * @private
 */
SelectionHandler.prototype.updatePreviewPanelVisibility_ = function() {
  var panel = this.previewPanel_;
  var state = panel.getAttribute('visibility');
  var mustBeVisible = (this.selection.totalCount > 0);
  var self = this;
  var fm = this.fileManager_;

  switch (state) {
    case 'visible':
      if (!mustBeVisible)
        startHiding();
      break;

    case 'hiding':
      if (mustBeVisible)
        stopHidingAndShow();
      break;

    case 'hidden':
      if (mustBeVisible)
        show();
  }

  function stopHidingAndShow() {
    clearTimeout(self.hidingTimeout_);
    self.hidingTimeout_ = 0;
    setVisibility('visible');
  }

  function startHiding() {
    setVisibility('hiding');
    self.hidingTimeout_ = setTimeout(function() {
        self.hidingTimeout_ = 0;
        setVisibility('hidden');
        fm.onResize_();
      }, 250);
  }

  function show() {
    setVisibility('visible');
    self.previewThumbnails_.textContent = '';
    fm.onResize_();
  }

  function setVisibility(visibility) {
    panel.setAttribute('visibility', visibility);
  }
};

/**
 * Update the selection summary in preview panel.
 * @private
 */
SelectionHandler.prototype.updatePreviewPanelText_ = function() {
  var selection = this.selection;
  var bytes = util.bytesToSi(selection.bytes);
  var text = '';
  if (selection.totalCount == 0) {
    // We dont want to change the string during preview panel animating away.
    return;
  } else if (selection.fileCount == 1 && selection.directoryCount == 0) {
    text = selection.entries[0].name;
    if (selection.showBytes) text += ', ' + bytes;
  } else if (selection.fileCount == 0 && selection.directoryCount == 1) {
    text = selection.entries[0].name;
  } else if (selection.directoryCount == 0) {
    text = strf('MANY_FILES_SELECTED', selection.fileCount, bytes);
    // TODO(dgozman): change the string to not contain ", $2".
    if (!selection.showBytes) text = text.substring(0, text.lastIndexOf(','));
  } else if (selection.fileCount == 0) {
    text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
  } else {
    text = strf('MANY_ENTRIES_SELECTED', selection.totalCount, bytes);
    // TODO(dgozman): change the string to not contain ", $2".
    if (!selection.showBytes) text = text.substring(0, text.lastIndexOf(','));
  }
  this.previewSummary_.textContent = text;
};

/**
 * Calculates async selection stats and updates secondary UI elements.
 * @param {Selection} selection The selection object.
 */
SelectionHandler.prototype.updateSelectionAsync = function(selection) {
  if (this.selection != selection) return;

  // Update the file tasks.
  var onTasks = function() {
    if (this.selection != selection) return;
    selection.tasks.display(this.taskItems_);
    selection.tasks.updateMenuItem();
  }.bind(this);

  if (this.fileManager_.dialogType == DialogType.FULL_PAGE &&
      selection.directoryCount == 0 && selection.fileCount > 0) {
    selection.createTasks(onTasks);
  } else {
    this.taskItems_.hidden = true;
  }

  // Update the UI.
  this.updatePreviewPanelVisibility_();
  this.updateSearchBreadcrumbs_();
  this.fileManager_.updateContextMenuActionItems(null, false);

  // Sync the commands availability.
  var commands = this.fileManager_.dialogDom_.querySelectorAll('command');
  for (var i = 0; i < commands.length; i++)
    commands[i].canExecuteChange();

  // Update the summary information.
  var onBytes = function() {
    if (this.selection != selection) return;
    this.updatePreviewPanelText_();
  }.bind(this);
  selection.computeBytes(onBytes);

  // Inform tests it's OK to click buttons now.
  chrome.test.sendMessage('selection-change-complete');

  // Show thumbnails.
  this.showPreviewThumbnails_(selection);
};

/**
 * Renders preview thumbnails in preview panel.
 * @param {Selection} selection The selection object.
 * @private
 */
SelectionHandler.prototype.showPreviewThumbnails_ = function(selection) {
  var thumbnails = [];
  var thumbnailCount = 0;
  var thumbnailLoaded = -1;
  var forcedShowTimeout = null;
  var thumbnailsHaveZoom = false;
  var self = this;

  function showThumbnails() {
    // have-zoom class may be updated twice: then timeout exceeds and then
    // then all images loaded.
    if (self.selection == selection) {
      if (thumbnailsHaveZoom) {
        self.previewThumbnails_.classList.add('has-zoom');
      } else {
        self.previewThumbnails_.classList.remove('has-zoom');
      }
    }

    if (forcedShowTimeout === null)
      return;
    clearTimeout(forcedShowTimeout);
    forcedShowTimeout = null;

    // Selection could change while images are loading.
    if (self.selection == selection) {
      self.previewThumbnails_.textContent = '';
      for (var i = 0; i < thumbnails.length; i++)
        self.previewThumbnails_.appendChild(thumbnails[i]);
    }
  }

  function onThumbnailLoaded() {
    thumbnailLoaded++;
    if (thumbnailLoaded == thumbnailCount)
      showThumbnails();
  }

  function thumbnailClickHandler() {
    if (selection.tasks)
      selection.tasks.executeDefault();
  }

  for (var i = 0; i < selection.entries.length; i++) {
    var entry = selection.entries[i];

    if (thumbnailCount < SelectionHandler.MAX_PREVIEW_THUMBNAIL_COUNT) {
      var box = this.fileManager_.document_.createElement('div');
      box.className = 'thumbnail';
      if (thumbnailCount == 0) {
        var zoomed = this.fileManager_.document_.createElement('div');
        zoomed.hidden = true;
        thumbnails.push(zoomed);
        function onFirstThumbnailLoaded(img, transform) {
          if (img && self.decorateThumbnailZoom_(zoomed, img, transform)) {
            zoomed.hidden = false;
            thumbnailsHaveZoom = true;
          }
          onThumbnailLoaded();
        }
        var thumbnail = this.fileManager_.renderThumbnailBox_(
            entry, true, onFirstThumbnailLoaded);
        zoomed.addEventListener('click', thumbnailClickHandler);
      } else {
        var thumbnail = this.fileManager_.renderThumbnailBox_(
            entry, true, onThumbnailLoaded);
      }
      thumbnailCount++;
      box.appendChild(thumbnail);
      box.style.zIndex = SelectionHandler.MAX_PREVIEW_THUMBNAIL_COUNT + 1 - i;
      box.addEventListener('click', thumbnailClickHandler);

      thumbnails.push(box);
    }
  }

  forcedShowTimeout = setTimeout(showThumbnails,
      FileManager.THUMBNAIL_SHOW_DELAY);
  onThumbnailLoaded();
};

/**
 * Updates the search breadcrumbs.
 * @private
 */
SelectionHandler.prototype.updateSearchBreadcrumbs_ = function() {
  var selectedIndexes =
      this.fileManager_.getCurrentList().selectionModel.selectedIndexes;
  if (selectedIndexes.length !== 1 ||
      !this.fileManager_.directoryModel_.isSearching()) {
    this.searchBreadcrumbs_.hide();
    return;
  }

  var entry = this.fileManager_.getFileList().item(
      selectedIndexes[0]);
  this.searchBreadcrumbs_.show(
      PathUtil.getRootPath(entry.fullPath),
      entry.fullPath);
};

/**
 * Creates enlarged image for a bottom pannel thumbnail.
 * Image's assumed to be just loaded and not inserted into the DOM.
 *
 * @param {HTMLElement} largeImageBox DIV element to decorate.
 * @param {HTMLElement} img Loaded image.
 * @param {Object} transform Image transformation description.
 * @return {boolean} True if zoomed image is present.
 * @private
 */
SelectionHandler.prototype.decorateThumbnailZoom_ = function(
    largeImageBox, img, transform) {
  var width = img.width;
  var height = img.height;
  var THUMBNAIL_SIZE = 45;
  if (width < THUMBNAIL_SIZE * 2 && height < THUMBNAIL_SIZE * 2)
    return false;

  var scale = Math.min(1,
      SelectionHandler.IMAGE_HOVER_PREVIEW_SIZE / Math.max(width, height));

  var imageWidth = Math.round(width * scale);
  var imageHeight = Math.round(height * scale);

  var largeImage = this.fileManager_.document_.createElement('img');
  if (scale < 0.3) {
    // Scaling large images kills animation. Downscale it in advance.

    // Canvas scales images with liner interpolation. Make a larger
    // image (but small enough to not kill animation) and let IMG
    // scale it smoothly.
    var INTERMEDIATE_SCALE = 3;
    var canvas = this.fileManager_.document_.createElement('canvas');
    canvas.width = imageWidth * INTERMEDIATE_SCALE;
    canvas.height = imageHeight * INTERMEDIATE_SCALE;
    var ctx = canvas.getContext('2d');
    ctx.drawImage(img, 0, 0, canvas.width, canvas.height);
    // Using bigger than default compression reduces image size by
    // several times. Quality degradation compensated by greater resolution.
    largeImage.src = canvas.toDataURL('image/jpeg', 0.6);
  } else {
    largeImage.src = img.src;
  }
  largeImageBox.className = 'popup';

  var boxWidth = Math.max(THUMBNAIL_SIZE, imageWidth);
  var boxHeight = Math.max(THUMBNAIL_SIZE, imageHeight);

  if (transform && transform.rotate90 % 2 == 1) {
    var t = boxWidth;
    boxWidth = boxHeight;
    boxHeight = t;
  }

  var style = largeImageBox.style;
  style.width = boxWidth + 'px';
  style.height = boxHeight + 'px';
  style.top = (-boxHeight + THUMBNAIL_SIZE) + 'px';

  var style = largeImage.style;
  style.width = imageWidth + 'px';
  style.height = imageHeight + 'px';
  style.left = (boxWidth - imageWidth) / 2 + 'px';
  style.top = (boxHeight - imageHeight) / 2 + 'px';
  style.position = 'relative';

  util.applyTransform(largeImage, transform);

  largeImageBox.appendChild(largeImage);
  largeImageBox.style.zIndex = 1000;
  return true;
};
