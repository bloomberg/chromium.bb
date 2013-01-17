// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The current selection object.
 * @param {FileManager} fileManager FileManager instance.
 * @param {Array.<number>} indexes Selected indexes.
 */
function FileSelection(fileManager, indexes) {
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
  this.cancelled_ = false;
  this.bytesKnown = false;

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
FileSelection.prototype.createTasks = function(callback) {
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
FileSelection.prototype.computeBytes = function(callback) {
  if (this.entries.length == 0) {
    this.bytesKnown = true;
    this.bytes = 0;
    return;
  }

  var countdown = this.entries.length;
  var pendingMetadataCount = 0;

  var maybeDone = function() {
    if (countdown == 0 && pendingMetadataCount == 0 && !this.cancelled_) {
      this.bytesKnown = true;
      callback();
    }
  }.bind(this);

  var onProps = function(filesystem) {
    this.bytes += filesystem.size;
    pendingMetadataCount--;
    maybeDone();
  }.bind(this);

  var onEntry = function(entry) {
    if (entry) {
      if (entry.isFile) {
        this.showBytes |= !FileType.isHosted(entry);
        pendingMetadataCount++;
        this.fileManager_.metadataCache_.get(entry, 'filesystem', onProps);
      }
    } else {
      countdown--;
      maybeDone();
    }
    return !this.cancelled_;
  }.bind(this);

  for (var index = 0; index < this.entries.length; index++) {
    util.forEachEntryInTree(this.entries[index], onEntry);
  }
};

/**
 * Cancels any async computation.
 * @private
 */
FileSelection.prototype.cancelComputing_ = function() {
  this.cancelled_ = true;
};

/**
 * This object encapsulates everything related to current selection.
 * @param {FileManager} fileManager File manager instance.
 */
function FileSelectionHandler(fileManager) {
  this.fileManager_ = fileManager;
  // TODO(dgozman): create a shared object with most of UI elements.
  this.okButton_ = fileManager.okButton_;
  this.filenameInput_ = fileManager.filenameInput_;

  this.previewPanel_ = fileManager.dialogDom_.querySelector('.preview-panel');
  this.previewThumbnails_ = this.previewPanel_.
      querySelector('.preview-thumbnails');
  this.previewSummary_ = this.previewPanel_.querySelector('.preview-summary');
  this.previewText_ = this.previewSummary_.querySelector('.preview-text');
  this.calculatingSize_ = this.previewSummary_.
      querySelector('.calculating-size');
  this.calculatingSize_.textContent = str('CALCULATING_SIZE');

  this.searchBreadcrumbs_ = fileManager.searchBreadcrumbs_;
  this.taskItems_ = fileManager.taskItems_;

  this.animationTimeout_ = null;
}

/**
 * Maximum amount of thumbnails in the preview pane.
 */
FileSelectionHandler.MAX_PREVIEW_THUMBNAIL_COUNT = 4;

/**
 * Maximum width or height of an image what pops up when the mouse hovers
 * thumbnail in the bottom panel (in pixels).
 */
FileSelectionHandler.IMAGE_HOVER_PREVIEW_SIZE = 200;

/**
 * Update the UI when the selection model changes.
 *
 * @param {cr.Event} event The change event.
 */
FileSelectionHandler.prototype.onFileSelectionChanged = function(event) {
  var indexes =
      this.fileManager_.getCurrentList().selectionModel.selectedIndexes;
  if (this.selection) this.selection.cancelComputing_();
  var selection = new FileSelection(this.fileManager_, indexes);
  this.selection = selection;

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
  }

  if (!indexes.length) {
    this.updatePreviewPanelVisibility_();
    this.updatePreviewPanelText_();
    this.fileManager_.updateContextMenuActionItems(null, false);
    return;
  }

  this.hideCalculating_();

  // The rest of the selection properties are computed via (sometimes lengthy)
  // asynchronous calls. We initiate these calls after a timeout. If the
  // selection is changing quickly we only do this once when it slows down.

  var updateDelay = 200;
  var now = Date.now();
  if (now > (this.lastFileSelectionTime_ || 0) + updateDelay) {
    // The previous selection change happened a while ago. Update the UI soon.
    updateDelay = 0;
  }
  this.lastFileSelectionTime_ = now;

  this.selectionUpdateTimer_ = setTimeout(function() {
    this.selectionUpdateTimer_ = null;
    if (this.selection == selection)
      this.updateFileSelectionAsync(selection);
  }.bind(this), updateDelay);
};

/**
 * Clears the primary UI selection elements.
 */
FileSelectionHandler.prototype.clearUI = function() {
  this.previewThumbnails_.textContent = '';
  this.previewText_.textContent = '';
  this.hideCalculating_();
  this.taskItems_.hidden = true;
  this.okButton_.disabled = true;
};

/**
 * Updates the Ok button enabled state.
 * @return {boolean} Whether button is enabled.
 */
FileSelectionHandler.prototype.updateOkButton = function() {
  var selectable;
  var dialogType = this.fileManager_.dialogType;

  if (dialogType == DialogType.SELECT_FOLDER) {
    // In SELECT_FOLDER mode, we allow to select current directory
    // when nothing is selected.
    selectable = this.selection.directoryCount <= 1 &&
        this.selection.fileCount == 0;
  } else if (dialogType == DialogType.SELECT_OPEN_FILE) {
    selectable = (this.isFileSelectionAvailable() &&
                  this.selection.directoryCount == 0 &&
                  this.selection.fileCount == 1);
  } else if (dialogType == DialogType.SELECT_OPEN_MULTI_FILE) {
    selectable = (this.isFileSelectionAvailable() &&
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
FileSelectionHandler.prototype.isFileSelectionAvailable = function() {
  return !this.fileManager_.isOnGData() ||
      !this.fileManager_.isOffline() ||
      this.selection.allGDataFilesPresent;
};

/**
 * Animates preview panel show/hide transitions.
 * @private
 */
FileSelectionHandler.prototype.updatePreviewPanelVisibility_ = function() {
  var panel = this.previewPanel_;
  var state = panel.getAttribute('visibility');
  var mustBeVisible = (this.selection.totalCount > 0);
  var self = this;
  var fm = this.fileManager_;

  var stopHidingAndShow = function() {
    clearTimeout(self.hidingTimeout_);
    self.hidingTimeout_ = 0;
    setVisibility('visible');
  };

  var startHiding = function() {
    setVisibility('hiding');
    self.hidingTimeout_ = setTimeout(function() {
        self.hidingTimeout_ = 0;
        setVisibility('hidden');
        fm.onResize_();
      }, 250);
  };

  var show = function() {
    setVisibility('visible');
    self.previewThumbnails_.textContent = '';
    fm.onResize_();
  };

  var setVisibility = function(visibility) {
    panel.setAttribute('visibility', visibility);
  };

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
};

/**
 * @return {boolean} True if space reserverd for the preview panel.
 * @private
 */
FileSelectionHandler.prototype.isPreviewPanelVisibile_ = function() {
  return this.previewPanel_.getAttribute('visibility') != 'hidden';
};

/**
 * Update the selection summary in preview panel.
 * @private
 */
FileSelectionHandler.prototype.updatePreviewPanelText_ = function() {
  var selection = this.selection;
  if (selection.totalCount == 0) {
    // We dont want to change the string during preview panel animating away.
    return;
  }

  var text = '';
  if (selection.totalCount == 1) {
    text = selection.entries[0].name;
  } else if (selection.directoryCount == 0) {
    text = strf('MANY_FILES_SELECTED', selection.fileCount);
  } else if (selection.fileCount == 0) {
    text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
  } else {
    text = strf('MANY_ENTRIES_SELECTED', selection.totalCount);
  }

  if (selection.bytesKnown) {
    this.hideCalculating_();
    if (selection.showBytes) {
      var bytes = util.bytesToSi(selection.bytes);
      text += ', ' + bytes;
    }
  } else {
    this.showCalculating_();
  }

  this.previewText_.textContent = text;
};

/**
 * Displays the 'calculating size' label.
 * @private
 */
FileSelectionHandler.prototype.showCalculating_ = function() {
  if (this.animationTimeout_) {
    clearTimeout(this.animationTimeout_);
    this.animationTimeout_ = null;
  }

  var dotCount = 0;

  var advance = function() {
    this.animationTimeout_ = setTimeout(advance, 1000);

    var s = this.calculatingSize_.textContent;
    s = s.replace(/(\.)+$/, '');
    for (var i = 0; i < dotCount; i++) {
      s += '.';
    }
    this.calculatingSize_.textContent = s;

    dotCount = (dotCount + 1) % 3;
  }.bind(this);

  var start = function() {
    this.calculatingSize_.hidden = false;
    advance();
  }.bind(this);

  this.animationTimeout_ = setTimeout(start, 500);
};

/**
 * Hides the 'calculating size' label.
 * @private
 */
FileSelectionHandler.prototype.hideCalculating_ = function() {
  if (this.animationTimeout_) {
    clearTimeout(this.animationTimeout_);
    this.animationTimeout_ = null;
  }
  this.calculatingSize_.hidden = true;
};

/**
 * Calculates async selection stats and updates secondary UI elements.
 * @param {FileSelection} selection The selection object.
 */
FileSelectionHandler.prototype.updateFileSelectionAsync = function(selection) {
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
  var wasVisible = this.isPreviewPanelVisibile_();
  this.updatePreviewPanelVisibility_();
  this.updateSearchBreadcrumbs_();
  this.fileManager_.updateContextMenuActionItems(null, false);
  if (!wasVisible && this.selection.totalCount == 1) {
    var list = this.fileManager_.getCurrentList();
    list.scrollIndexIntoView(list.selectionModel.selectedIndex);
  }

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
  this.updatePreviewPanelText_();

  // Inform tests it's OK to click buttons now.
  chrome.test.sendMessage('selection-change-complete');

  // Show thumbnails.
  this.showPreviewThumbnails_(selection);
};

/**
 * Renders preview thumbnails in preview panel.
 * @param {FileSelection} selection The selection object.
 * @private
 */
FileSelectionHandler.prototype.showPreviewThumbnails_ = function(selection) {
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

    // FileSelection could change while images are loading.
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

  var doc = this.fileManager_.document_;
  for (var i = 0; i < selection.entries.length; i++) {
    var entry = selection.entries[i];

    if (thumbnailCount < FileSelectionHandler.MAX_PREVIEW_THUMBNAIL_COUNT) {
      var box = doc.createElement('div');
      box.className = 'thumbnail';
      if (thumbnailCount == 0) {
        var zoomed = doc.createElement('div');
        zoomed.hidden = true;
        thumbnails.push(zoomed);
        function onFirstThumbnailLoaded(img, transform) {
          if (img && self.decorateThumbnailZoom_(zoomed, img, transform)) {
            zoomed.hidden = false;
            thumbnailsHaveZoom = true;
          }
          onThumbnailLoaded();
        }
        var thumbnail = this.renderThumbnail_(entry, onFirstThumbnailLoaded);
        zoomed.addEventListener('click', thumbnailClickHandler);
      } else {
        var thumbnail = this.renderThumbnail_(entry, onThumbnailLoaded);
      }
      thumbnailCount++;
      box.appendChild(thumbnail);
      box.style.zIndex =
          FileSelectionHandler.MAX_PREVIEW_THUMBNAIL_COUNT + 1 - i;
      box.addEventListener('click', thumbnailClickHandler);

      thumbnails.push(box);
    }
  }

  forcedShowTimeout = setTimeout(showThumbnails,
      FileManager.THUMBNAIL_SHOW_DELAY);
  onThumbnailLoaded();
};

/**
 * Renders a thumbnail for the buttom panel.
 * @param {Entry} entry Entry to render for.
 * @param {Function} callback Callend when image loaded.
 * @return {HTMLDivElement} Created element.
 * @private
 */
FileSelectionHandler.prototype.renderThumbnail_ = function(entry, callback) {
  var thumbnail = this.fileManager_.document_.createElement('div');
  FileGrid.decorateThumbnailBox(thumbnail, entry,
      this.fileManager_.metadataCache_, true, callback);
  return thumbnail;
};

/**
 * Updates the search breadcrumbs.
 * @private
 */
FileSelectionHandler.prototype.updateSearchBreadcrumbs_ = function() {
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
FileSelectionHandler.prototype.decorateThumbnailZoom_ = function(
    largeImageBox, img, transform) {
  var width = img.width;
  var height = img.height;
  var THUMBNAIL_SIZE = 45;
  if (width < THUMBNAIL_SIZE * 2 && height < THUMBNAIL_SIZE * 2)
    return false;

  var scale = Math.min(1,
      FileSelectionHandler.IMAGE_HOVER_PREVIEW_SIZE / Math.max(width, height));

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
