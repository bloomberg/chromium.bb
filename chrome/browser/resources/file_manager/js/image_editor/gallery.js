// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Base class that Ribbon uses to display photos.
 */

function RibbonClient() {}

RibbonClient.prototype.prefetchImage = function(id, url) {};

RibbonClient.prototype.openImage = function(id, url, metadata, direction) {
};

RibbonClient.prototype.closeImage = function(item) {};

/**
 * Image gallery for viewing and editing image files.
 *
 * @param {HTMLDivElement} container
 * @param {Object} context Object containing the following:
 *     {function(string)} onNameChange Called every time a selected
 *         item name changes (on rename and on selection change).
 *     {function} onClose
 *     {MetadataProvider} metadataProvider
 *     {Array.<Object>} shareActions
 *     {string} readonlyDirName Directory name for readonly warning or null.
 *     {DirEntry} saveDirEntry Directory to save to.
 *     {function(string)} displayStringFunction
 */
function Gallery(container, context) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.context_ = context;

  var strf = context.displayStringFunction;
  this.displayStringFunction_ = function(id, formatArgs) {
    var args = Array.prototype.slice.call(arguments);
    args[0] = 'GALLERY_' + id.toUpperCase();
    return strf.apply(null, args);
  };

  this.onFadeTimeoutBound_ = this.onFadeTimeout_.bind(this);
  this.fadeTimeoutId_ = null;
  this.mouseOverTool_ = false;

  this.initDom_();
}

Gallery.prototype = { __proto__: RibbonClient.prototype };

Gallery.open = function(context, items, selectedItem) {
  var container = document.querySelector('.gallery');
  ImageUtil.removeChildren(container);
  var gallery = new Gallery(container, context);
  gallery.load(items, selectedItem);
};

Gallery.editorModes = [
  new ImageEditor.Mode.InstantAutofix(),
  new ImageEditor.Mode.Crop(),
  new ImageEditor.Mode.Exposure(),
  new ImageEditor.Mode.OneClick('rotate_left', new Command.Rotate(-1)),
  new ImageEditor.Mode.OneClick('rotate_right', new Command.Rotate(1))
];

Gallery.FADE_TIMEOUT = 3000;
Gallery.FIRST_FADE_TIMEOUT = 1000;
Gallery.OVERWRITE_BUBBLE_MAX_TIMES = 5;

Gallery.prototype.initDom_ = function() {
  var doc = this.document_;

  doc.oncontextmenu = function(e) { e.preventDefault(); };

  doc.body.addEventListener('keydown', this.onKeyDown_.bind(this));

  doc.body.addEventListener('mousemove', this.onMouseMove_.bind(this));

  window.addEventListener('unload', this.onUnload_.bind(this));

  // We need to listen to the top window 'unload' and 'beforeunload' because
  // the Gallery iframe does not get notified if the tab is closed.
  this.onTopUnloadBound_ = this.onTopUnload_.bind(this);
  window.top.addEventListener('unload', this.onTopUnloadBound_);

  this.onBeforeUnloadBound_ = this.onBeforeUnload_.bind(this);
  window.top.addEventListener('beforeunload', this.onBeforeUnloadBound_);

  this.closeButton_ = doc.createElement('div');
  this.closeButton_.className = 'close tool dimmable';
  this.closeButton_.appendChild(doc.createElement('div'));
  this.closeButton_.addEventListener('click', this.onClose_.bind(this));
  this.container_.appendChild(this.closeButton_);

  this.imageContainer_ = doc.createElement('div');
  this.imageContainer_.className = 'image-container';
  this.imageContainer_.addEventListener('click', (function() {
    this.filenameEdit_.blur();
    if (this.isShowingVideo_())
      this.mediaControls_.togglePlayStateWithFeedback();
  }).bind(this));
  this.container_.appendChild(this.imageContainer_);

  this.toolbar_ = doc.createElement('div');
  this.toolbar_.className = 'toolbar tool dimmable';
  this.container_.appendChild(this.toolbar_);

  this.filenameSpacer_ = doc.createElement('div');
  this.filenameSpacer_.className = 'filename-spacer';
  this.toolbar_.appendChild(this.filenameSpacer_);

  this.bubble_ = doc.createElement('div');
  this.bubble_.className = 'bubble';
  var bubbleContent = doc.createElement('div');
  bubbleContent.innerHTML = this.displayStringFunction_('overwrite_bubble');
  this.bubble_.appendChild(bubbleContent);
  var bubblePointer = doc.createElement('span');
  bubblePointer.className = 'pointer bottom';
  this.bubble_.appendChild(bubblePointer);
  var bubbleClose = doc.createElement('div');
  bubbleClose.className = 'close-x';
  bubbleClose.addEventListener('click', this.onCloseBubble_.bind(this));
  this.bubble_.appendChild(bubbleClose);
  this.bubble_.setAttribute('hidden', 'hidden');
  this.toolbar_.appendChild(this.bubble_);

  var nameBox = doc.createElement('div');
  nameBox.className = 'namebox';
  this.filenameSpacer_.appendChild(nameBox);

  this.filenameText_ = doc.createElement('div');
  this.filenameText_.addEventListener('click',
      this.onFilenameClick_.bind(this));
  nameBox.appendChild(this.filenameText_);

  this.filenameEdit_ = doc.createElement('input');
  this.filenameEdit_.setAttribute('type', 'text');
  this.filenameEdit_.addEventListener('blur',
      this.onFilenameEditBlur_.bind(this));
  this.filenameEdit_.addEventListener('keydown',
      this.onFilenameEditKeydown_.bind(this));
  nameBox.appendChild(this.filenameEdit_);

  var options = doc.createElement('div');
  options.className = 'options';
  this.filenameSpacer_.appendChild(options);

  this.savedLabel_ = doc.createElement('div');
  this.savedLabel_.className = 'saved';
  this.savedLabel_.textContent = this.displayStringFunction_('saved');
  options.appendChild(this.savedLabel_);

  var overwriteOriginalBox = doc.createElement('div');
  overwriteOriginalBox.className = 'overwrite-original';
  options.appendChild(overwriteOriginalBox);

  this.overwriteOriginal_ = doc.createElement('input');
  this.overwriteOriginal_.type = 'checkbox';
  this.overwriteOriginal_.id = 'overwrite-checkbox';
  this.overwriteOriginal_.className = 'common white';
  overwriteOriginalBox.appendChild(this.overwriteOriginal_);
  this.overwriteOriginal_.addEventListener('click',
      this.onOverwriteOriginalClick_.bind(this));

  var overwriteLabel = doc.createElement('label');
  overwriteLabel.textContent =
      this.displayStringFunction_('overwrite_original');
  overwriteLabel.setAttribute('for', 'overwrite-checkbox');
  overwriteOriginalBox.appendChild(overwriteLabel);

  this.buttonSpacer_ = doc.createElement('div');
  this.buttonSpacer_.className = 'button-spacer';
  this.toolbar_.appendChild(this.buttonSpacer_);

  this.ribbonSpacer_ = doc.createElement('div');
  this.ribbonSpacer_.className = 'ribbon-spacer';
  this.toolbar_.appendChild(this.ribbonSpacer_);

  this.mediaSpacer_ = doc.createElement('div');
  this.mediaSpacer_.className = 'video-controls-spacer';
  this.container_.appendChild(this.mediaSpacer_);

  this.mediaToolbar_ = doc.createElement('div');
  this.mediaToolbar_.className = 'tool';
  this.mediaSpacer_.appendChild(this.mediaToolbar_);

  this.mediaControls_ = new VideoControls(
      this.mediaToolbar_,
      this.showErrorBanner_.bind(this, 'VIDEO_ERROR'),
      this.toggleFullscreen_.bind(this),
      this.container_);

  this.arrowBox_ = this.document_.createElement('div');
  this.arrowBox_.className = 'arrow-box';
  this.container_.appendChild(this.arrowBox_);

  this.arrowLeft_ = this.document_.createElement('div');
  this.arrowLeft_.className = 'arrow left tool dimmable';
  this.arrowLeft_.appendChild(doc.createElement('div'));
  this.arrowBox_.appendChild(this.arrowLeft_);

  this.arrowSpacer_ = this.document_.createElement('div');
  this.arrowSpacer_.className = 'arrow-spacer';
  this.arrowBox_.appendChild(this.arrowSpacer_);

  this.arrowRight_ = this.document_.createElement('div');
  this.arrowRight_.className = 'arrow right tool dimmable';
  this.arrowRight_.appendChild(doc.createElement('div'));
  this.arrowBox_.appendChild(this.arrowRight_);

  this.spinner_ = this.document_.createElement('div');
  this.spinner_.className = 'spinner';
  this.container_.appendChild(this.spinner_);

  this.errorWrapper_ = this.document_.createElement('div');
  this.errorWrapper_.className = 'prompt-wrapper';
  this.errorWrapper_.setAttribute('pos', 'center');
  this.container_.appendChild(this.errorWrapper_);

  this.errorBanner_ = this.document_.createElement('div');
  this.errorBanner_.className = 'error-banner';
  this.errorWrapper_.appendChild(this.errorBanner_);

  this.ribbon_ = new Ribbon(this.ribbonSpacer_,
      this, this.context_.metadataProvider, this.arrowLeft_, this.arrowRight_);

  this.editBar_  = doc.createElement('div');
  this.editBar_.className = 'edit-bar';
  this.toolbar_.appendChild(this.editBar_);

  this.editButton_ = doc.createElement('div');
  this.editButton_.className = 'button edit';
  this.editButton_.textContent = this.displayStringFunction_('edit');
  this.editButton_.addEventListener('click', this.onEdit_.bind(this));
  this.toolbar_.appendChild(this.editButton_);

  this.editBarMain_  = doc.createElement('div');
  this.editBarMain_.className = 'edit-main';
  this.editBar_.appendChild(this.editBarMain_);

  this.editBarMode_  = doc.createElement('div');
  this.editBarMode_.className = 'edit-modal';
  this.container_.appendChild(this.editBarMode_);

  this.editBarModeWrapper_  = doc.createElement('div');
  ImageUtil.setAttribute(this.editBarModeWrapper_, 'hidden', true);
  this.editBarModeWrapper_.className = 'edit-modal-wrapper';
  this.editBarMode_.appendChild(this.editBarModeWrapper_);

  this.viewport_ = new Viewport();

  this.imageView_ = new ImageView(
      this.imageContainer_,
      this.viewport_,
      this.context_.metadataProvider);

  this.editor_ = new ImageEditor(
      this.viewport_,
      this.imageView_,
      {
        root: this.container_,
        image: this.imageContainer_,
        toolbar: this.editBarMain_,
        mode: this.editBarModeWrapper_
      },
      Gallery.editorModes,
      this.displayStringFunction_);

  this.imageView_.addContentCallback(this.onImageContentChanged_.bind(this));

  this.editor_.trackWindow(doc.defaultView);

  Gallery.getFileBrowserPrivate().isFullscreen(function(fullscreen) {
    this.originalFullscreen_ = fullscreen;
  }.bind(this));
};

Gallery.prototype.onBeforeUnload_ = function(event) {
  if (this.editor_.isBusy())
    return this.displayStringFunction_('unsaved_changes');
  return null;
};

Gallery.prototype.load = function(items, selectedItem) {
  var urls = [];
  var selectedIndex = -1;

  // Convert canvas and blob items to blob urls.
  for (var i = 0; i != items.length; i++) {
    var item = items[i];
    var selected = (item == selectedItem);

    if (typeof item == 'string') {
      if (selected) selectedIndex = urls.length;
      urls.push(item);
    } else {
      console.error('Unsupported item type', item);
    }
  }

  if (urls.length == 0)
    throw new Error('Cannot open the gallery for 0 items');

  if (selectedIndex == -1)
    throw new Error('Cannot find selected item');

  var self = this;

  var selectedURL = urls[selectedIndex];

  function initRibbon() {
    self.ribbon_.load(urls, selectedIndex);
    if (urls.length == 1 && self.isShowingVideo_()) {
      // We only have one item and it is a video. Move the playback controls
      // in place of thumbnails and start the playback immediately.
      self.mediaSpacer_.removeChild(self.mediaToolbar_);
      self.ribbonSpacer_.appendChild(self.mediaToolbar_);
      self.mediaControls_.play();
    }
    // Flash the toolbar briefly to let the user know it is there.
    self.cancelFading_();
    self.initiateFading_(Gallery.FIRST_FADE_TIMEOUT);
  }

  // Show the selected item ASAP, then complete the initialization (populating
  // the ribbon can take especially long time).
  this.context_.metadataProvider.fetch(selectedURL, function (metadata) {
    self.openImage(selectedIndex, selectedURL, metadata, 0, initRibbon);
  });

  this.context_.getShareActions(urls, function(tasks) {
    if (tasks.length > 0) {
      this.shareMode_ = new ShareMode(this.editor_, this.container_,
        this.toolbar_, tasks,
        this.onShare_.bind(this), this.onActionExecute_.bind(this),
        this.displayStringFunction_);
    } else {
      this.shareMode_ = null;
    }
  }.bind(this));
};

Gallery.prototype.onImageContentChanged_ = function() {
  var revision = this.imageView_.getContentRevision();
  if (revision == 0) return;  // Just loaded.

  if (revision == 1) {
    // First edit.
    this.filenameSpacer_.setAttribute('saved', 'saved');
    this.filenameSpacer_.setAttribute('overwrite', 'overwrite');

    var key = 'gallery-overwrite-original';
    var overwrite = key in localStorage ? (localStorage[key] == "true") : true;
    this.overwriteOriginal_.checked = overwrite;
    this.applyOverwrite_(overwrite);

    key = 'gallery-overwrite-bubble';
    var times = key in localStorage ? parseInt(localStorage[key], 10) : 0;
    if (times < Gallery.OVERWRITE_BUBBLE_MAX_TIMES) {
      this.bubble_.removeAttribute('hidden');
      localStorage[key] = times + 1;
    }

    ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('Edit'));
  }
};

Gallery.prototype.flashSavedLabel_ = function() {
  var label = this.savedLabel_;
  setTimeout(function(){ label.setAttribute('highlighted', 'true'); }, 0);
  setTimeout(function(){ label.removeAttribute('highlighted'); }, 300);
};

Gallery.prototype.applyOverwrite_ = function(overwrite) {
  if (overwrite) {
    this.ribbon_.getSelectedItem().setOriginalName(this.context_.saveDirEntry,
        this.updateFilename_.bind(this));
  } else {
    this.ribbon_.getSelectedItem().setCopyName(this.context_.saveDirEntry,
        this.selectedImageMetadata_,
        this.updateFilename_.bind(this));
  }
};

Gallery.prototype.onOverwriteOriginalClick_ = function(event) {
  var overwrite = event.target.checked;
  localStorage['gallery-overwrite-original'] = overwrite;
  this.applyOverwrite_(overwrite);
};

Gallery.prototype.onCloseBubble_ = function(event) {
  this.bubble_.setAttribute('hidden', 'hidden');
  localStorage['gallery-overwrite-bubble'] = Gallery.OVERWRITE_BUBBLE_MAX_TIMES;
};

Gallery.prototype.saveCurrentImage_ = function(callback) {
  var item = this.ribbon_.getSelectedItem();
  var canvas = this.imageView_.getCanvas();

  this.showSpinner_(true);
  var metadataEncoder = ImageEncoder.encodeMetadata(
      this.selectedImageMetadata_, canvas, 1 /* quality */);

  this.selectedImageMetadata_ = metadataEncoder.getMetadata();
  item.setThumbnail(this.selectedImageMetadata_);

  item.saveToFile(
      this.context_.saveDirEntry,
      canvas,
      metadataEncoder,
      function(success) {
        // TODO(kaznacheev): Implement write error handling.
        // Until then pretend that the save succeeded.
        this.showSpinner_(false);
        this.flashSavedLabel_();
        this.context_.metadataProvider.reset(item.getUrl());
        callback();
      }.bind(this));
};

Gallery.prototype.onActionExecute_ = function(action) {
  // |executeWhenReady| closes the sharing menu.
  this.editor_.executeWhenReady(function() {
    action.execute([this.ribbon_.getSelectedItem().getUrl()]);
  }.bind(this));
};

Gallery.prototype.updateFilename_ = function(opt_url) {
  var fullName;

  var item = this.ribbon_.getSelectedItem();
  if (item) {
    fullName = item.getNameAfterSaving();
  } else if (opt_url) {
    fullName = ImageUtil.getFullNameFromUrl(opt_url);
  } else {
    return;
  }

  this.context_.onNameChange(fullName);

  var displayName = ImageUtil.getFileNameFromFullName(fullName);
  this.filenameEdit_.value = displayName;
  this.filenameText_.textContent = displayName;
};

Gallery.prototype.onFilenameClick_ = function() {
  // We can't rename files in readonly directory.
  if (this.context_.readonlyDirName)
    return;

  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', true);
  setTimeout(this.filenameEdit_.select.bind(this.filenameEdit_), 0);
  this.cancelFading_();
};

Gallery.prototype.onFilenameEditBlur_ = function() {
  if (this.filenameEdit_.value && this.filenameEdit_.value[0] == '.') {
    this.editor_.getPrompt().show('file_hidden_name', 5000);
    this.filenameEdit_.focus();
    return;
  }

  if (this.filenameEdit_.value) {
    this.renameItem_(this.ribbon_.getSelectedItem(),
        this.filenameEdit_.value);
  }

  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', false);
  this.initiateFading_();
};

Gallery.prototype.onFilenameEditKeydown_ = function() {
  switch (event.keyCode) {
    case 27:  // Escape
      this.updateFilename_();
      this.filenameEdit_.blur();
      break;

    case 13:  // Enter
      this.filenameEdit_.blur();
      break;
  }
  event.stopPropagation();
};

Gallery.prototype.renameItem_ = function(item, name) {
  var dir = this.context_.saveDirEntry;
  var self = this;
  var originalName = item.getNameAfterSaving();
  if (ImageUtil.getExtensionFromFullName(name) ==
      ImageUtil.getExtensionFromFullName(originalName)) {
    name = ImageUtil.getFileNameFromFullName(name);
  }
  var newName = ImageUtil.replaceFileNameInFullName(originalName, name);
  if (originalName == newName) return;

  function onError() {
    console.log('Rename error: "' + originalName + '" to "' + newName + '"');
  }

  function onSuccess(entry) {
    item.setUrl(entry.toURL());
    self.updateFilename_();
  }

  function doRename() {
    if (item.hasNameForSaving()) {
      // Use this name in the next save operation.
      item.setNameForSaving(newName);
      self.filenameSpacer_.removeAttribute('overwrite');
      self.updateFilename_();
    } else {
      // Rename file in place.
      dir.getFile(
          ImageUtil.getFullNameFromUrl(item.getUrl()),
          {create: false},
          function(entry) { entry.moveTo(dir, newName, onSuccess, onError); },
          onError);
    }
  }

  function onVictimFound(victim) {
    self.editor_.getPrompt().show('file_exists', 3000);
    self.filenameEdit_.value = name;
    self.onFilenameClick_();
  }

  dir.getFile(newName, {create: false, exclusive: false},
      onVictimFound, doRename);
};

Gallery.prototype.isRenaming_ = function() {
  return this.filenameSpacer_.hasAttribute('renaming');
};

Gallery.getFileBrowserPrivate = function() {
  return chrome.fileBrowserPrivate || window.top.chrome.fileBrowserPrivate;
};

Gallery.prototype.toggleFullscreen_ = function() {
  Gallery.getFileBrowserPrivate().toggleFullscreen();
};

/**
 * Close the Gallery.
 */
Gallery.prototype.close_ = function() {
  Gallery.getFileBrowserPrivate().isFullscreen(function(fullscreen) {
    if (this.originalFullscreen_ != fullscreen) {
      Gallery.getFileBrowserPrivate().toggleFullscreen();
    }
    this.context_.onClose();
  }.bind(this));
};

/**
 * Handle user's 'Close' action (Escape or a click on the X icon).
 */
Gallery.prototype.onClose_ = function() {
  // TODO: handle write errors gracefully (suggest retry or saving elsewhere).
  this.editor_.executeWhenReady(this.close_.bind(this));
};

Gallery.prototype.prefetchImage = function(id, url) {
  this.editor_.prefetchImage(id, url);
};

Gallery.prototype.openImage = function(id, url, metadata, slide, callback) {
  this.filenameSpacer_.removeAttribute('overwrite');
  this.filenameSpacer_.removeAttribute('saved');

  this.selectedImageMetadata_ = metadata;
  this.updateFilename_(url);

  this.showSpinner_(true);

  var self = this;
  function loadDone(loadType) {
    var video = self.isShowingVideo_();
    ImageUtil.setAttribute(self.container_, 'video', video);

    self.showSpinner_(false);
    if (loadType == ImageView.LOAD_TYPE_ERROR) {
      self.showErrorBanner_(video? 'VIDEO_ERROR' : 'IMAGE_ERROR');
    }

    if (video) {
      if (self.isEditing_()) {
        // The editor toolbar does not make sense for video, hide it.
        self.onEdit_();
      }
      self.mediaControls_.attachMedia(self.imageView_.getVideo());
      //TODO(kaznacheev): Add metrics for video playback.
    } else {
      ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('View'));

      function toMillions(number) { return Math.round(number / (1000 * 1000)) }

      ImageUtil.metrics.recordSmallCount(ImageUtil.getMetricName('Size.MB'),
          toMillions(metadata.fileSize));

      var canvas = self.imageView_.getCanvas();
      ImageUtil.metrics.recordSmallCount(ImageUtil.getMetricName('Size.MPix'),
          toMillions(canvas.width * canvas.height));

      var extIndex = url.lastIndexOf('.');
      var ext = extIndex < 0 ? '' : url.substr(extIndex + 1).toLowerCase();
      if (ext == 'jpeg') ext = 'jpg';
      ImageUtil.metrics.recordEnum(
          ImageUtil.getMetricName('FileType'), ext, ImageUtil.FILE_TYPES);
    }

    callback(loadType);
  }

  this.editor_.openSession(
      id, url, metadata, slide, this.saveCurrentImage_.bind(this), loadDone);
};

Gallery.prototype.closeImage = function(callback) {
  this.showSpinner_(false);
  this.showErrorBanner_(false);
  this.editor_.getPrompt().hide();
  if (this.isShowingVideo_()) {
    this.mediaControls_.pause();
    this.mediaControls_.detachMedia();
  }
  this.editor_.closeSession(callback);
};

Gallery.prototype.showSpinner_ = function(on) {
  if (this.spinnerTimer_) {
    clearTimeout(this.spinnerTimer_);
    this.spinnerTimer_ = null;
  }

  if (on) {
    this.spinnerTimer_ = setTimeout(function() {
      this.spinnerTimer_ = null;
      ImageUtil.setAttribute(this.container_, 'spinner', true);
    }.bind(this), 1000);
  } else {
    ImageUtil.setAttribute(this.container_, 'spinner', false);
  }
};

Gallery.prototype.showErrorBanner_ = function(message) {
  if (message) {
    this.errorBanner_.textContent = this.displayStringFunction_(message);
  }
  ImageUtil.setAttribute(this.container_, 'error', !!message);
};

Gallery.prototype.isShowingVideo_ = function() {
  return !!this.imageView_.getVideo();
};

Gallery.prototype.saveVideoPosition_ = function() {
  if (this.isShowingVideo_() && this.mediaControls_.isPlaying()) {
    this.mediaControls_.savePosition();
  }
};

Gallery.prototype.onUnload_ = function() {
  this.saveVideoPosition_();
  window.top.removeEventListener('beforeunload', this.onBeforeUnloadBound_);
  window.top.removeEventListener('unload', this.onTopUnloadBound_);
};

Gallery.prototype.onTopUnload_ = function() {
  this.saveVideoPosition_();
};

Gallery.prototype.isEditing_ = function() {
  return this.container_.hasAttribute('editing');
};

Gallery.prototype.onEdit_ = function() {
  ImageUtil.setAttribute(this.container_, 'editing', !this.isEditing_());

  // The user has just clicked on the Edit button. Dismiss the Share menu.
  if (this.isSharing_()) {
    this.onShare_();
  }

  // isEditing_ has just been flipped to a new value.
  if (this.isEditing_()) {
    if (this.context_.readonlyDirName) {
      this.editor_.getPrompt().showAt(
          'top', 'readonly_warning', 0, this.context_.readonlyDirName);
    }
    this.cancelFading_();
  } else {
    this.editor_.getPrompt().hide();
    this.filenameSpacer_.removeAttribute('saved');
    this.filenameSpacer_.removeAttribute('overwrite');
    this.initiateFading_();
  }

  ImageUtil.setAttribute(this.editButton_, 'pressed', this.isEditing_());
};

Gallery.prototype.isSharing_ = function(event) {
  return this.shareMode_ && this.shareMode_ == this.editor_.getMode();
};

Gallery.prototype.onShare_ = function(event) {
  this.editor_.enterMode(this.shareMode_, event);
  if (this.isSharing_()) {
    this.cancelFading_();
  } else {
    this.initiateFading_();
  }
};

Gallery.prototype.onKeyDown_ = function(event) {
  if (this.isEditing_() && this.editor_.onKeyDown(event))
    return;

  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+0008': // Backspace.
      // The default handler would call history.back and close the Gallery.
      event.preventDefault();
      break;

    case 'U+001B':  // Escape
      if (this.isEditing_()) {
        this.onEdit_();
      } else if (this.isSharing_()) {
        this.onShare_();
      } else {
        this.onClose_();
      }
      break;

    case 'U+0045':  // 'e' toggles the editor
      if (!this.isShowingVideo_()) {
        this.onEdit_();
      }
      break;

    case 'U+0020':  // Space toggles the video playback.
      if (this.isShowingVideo_()) {
        this.mediaControls_.togglePlayStateWithFeedback();
      }
      break;

    case 'Home':
      this.ribbon_.selectFirst();
      break;
    case 'Left':
      this.ribbon_.selectNext(-1);
      break;
    case 'Right':
      this.ribbon_.selectNext(1);
      break;
    case 'End':
      this.ribbon_.selectLast();
      break;

    case 'Ctrl-U+00DD':  // Ctrl+] (cryptic on purpose).
      this.ribbon_.toggleDebugSlideshow();
      break;
  }
};

Gallery.prototype.onMouseMove_ = function(e) {
  if (this.clientX_ == e.clientX && this.clientY_ == e.clientY) {
    // The mouse has not moved, must be the cursor change triggered by
    // some of the attributes on the root container. Ignore the event.
    return;
  }
  this.clientX_ = e.clientX;
  this.clientY_ = e.clientY;

  this.cancelFading_();

  this.mouseOverTool_ = false;
  for (var elem = e.target; elem != this.container_; elem = elem.parentNode) {
    if (elem.classList.contains('tool')) {
      this.mouseOverTool_ = true;
      break;
    }
  }

  this.initiateFading_();
};

Gallery.prototype.onFadeTimeout_ = function() {
  this.fadeTimeoutId_ = null;
  if (this.isEditing_() || this.isSharing_() || this.isRenaming_()) return;
  this.container_.removeAttribute('tools');
};

Gallery.prototype.initiateFading_ = function(opt_timeout) {
  if (this.mouseOverTool_ || this.isEditing_() || this.isSharing_() ||
      this.isRenaming_())
    return;

  if (!this.fadeTimeoutId_)
    this.fadeTimeoutId_ = window.setTimeout(
        this.onFadeTimeoutBound_, opt_timeout || Gallery.FADE_TIMEOUT);
};

Gallery.prototype.cancelFading_ = function() {
  ImageUtil.setAttribute(this.container_, 'tools', true);

  if (this.fadeTimeoutId_) {
    window.clearTimeout(this.fadeTimeoutId_);
    this.fadeTimeoutId_ = null;
  }
};

/**
 * @param {HTMLElement} container
 * @param {RibbonClient} client
 * @param {MetadataProvider} metadataProvider
 * @param {HTMLElement} arrowLeft
 * @param {HTMLElement} arrowRight
 */
function Ribbon(container, client, metadataProvider, arrowLeft, arrowRight) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.client_ = client;
  this.metadataProvider_ = metadataProvider;

  this.items_ = [];
  this.selectedIndex_ = -1;

  this.arrowLeft_ = arrowLeft;
  this.arrowLeft_.
      addEventListener('click', this.selectNext.bind(this, -1, null));

  this.arrowRight_ = arrowRight;
  this.arrowRight_.
      addEventListener('click', this.selectNext.bind(this, 1, null));

  this.bar_ = this.document_.createElement('div');
  this.bar_.className = 'ribbon';
  this.container_.appendChild(this.bar_);
}

Ribbon.PAGING_SINGLE_ITEM_DELAY = 20;
Ribbon.PAGING_ANIMATION_DURATION = 200;

Ribbon.prototype.getSelectedItem = function () {
  return this.items_[this.selectedIndex_];
};

Ribbon.prototype.clear = function() {
  this.bar_.textContent = '';
  this.items_ = [];
  this.selectedIndex_ = -1;
  this.firstVisibleIndex_ = 0;
  this.lastVisibleIndex_ = -1;  // Zero thumbnails
  this.sequenceDirection_ = 0;
  this.sequenceLength_ = 0;
};

Ribbon.prototype.add = function(url) {
  var index = this.items_.length;
  var selectClosure = this.select.bind(this, index, 0, null);
  var item = new Ribbon.Item(index, url, this.document_, selectClosure);
  this.items_.push(item);
};

Ribbon.prototype.load = function(urls, selectedIndex) {
  this.clear();
  for (var index = 0; index < urls.length; ++index) {
    this.add(urls[index]);
  }
  this.selectedIndex_ = selectedIndex;

  // We do not want to call this.select because the selected image is already
  // displayed. Instead we just update the UI.
  this.getSelectedItem().select(true);
  this.redraw();

  // Let the thumbnails load before prefetching the next image.
  setTimeout(this.requestPrefetch.bind(this, 1), 1000);

  // Make the arrows visible if there are more than 1 image.
  ImageUtil.setAttribute(this.arrowLeft_, 'active', this.items_.length > 1);
  ImageUtil.setAttribute(this.arrowRight_, 'active', this.items_.length > 1);
};

Ribbon.prototype.select = function(index, opt_forceStep, opt_callback) {
  if (index == this.selectedIndex_)
    return;  // Do not reselect.

  this.client_.closeImage(
      this.doSelect_.bind(this, index, opt_forceStep, opt_callback));
};

Ribbon.prototype.doSelect_ = function(index, opt_forceStep, opt_callback) {
  if (index == this.selectedIndex_)
    return;  // Do not reselect

  var selectedItem = this.getSelectedItem();
  selectedItem.select(false);

  var step = opt_forceStep || (index - this.selectedIndex_);

  if (Math.abs(step) != 1) {
    // Long leap, the sequence is broken, we have no good prefetch candidate.
    this.sequenceDirection_ = 0;
    this.sequenceLength_ = 0;
  } else if (this.sequenceDirection_ == step) {
    // Keeping going in sequence.
    this.sequenceLength_++;
  } else {
    // Reversed the direction. Reset the counter.
    this.sequenceDirection_ = step;
    this.sequenceLength_ = 1;
  }

  if (this.sequenceLength_ <= 1) {
    // We have just broke the sequence. Touch the current image so that it stays
    // in the cache longer.
    this.client_.prefetchImage(selectedItem.getIndex(), selectedItem.getUrl());
  }

  this.selectedIndex_ = index;

  selectedItem = this.getSelectedItem();
  selectedItem.select(true);
  this.redraw();

  function shouldPrefetch(loadType, step, sequenceLength) {
    // Never prefetch when selecting out of sequence.
    if (Math.abs(step) != 1)
      return false;

    // Never prefetch after a video load (decoding the next image can freeze
    // the UI for a second or two).
    if (loadType == ImageView.LOAD_TYPE_VIDEO_FILE)
      return false;

    // Always prefetch if the previous load was from cache.
    if (loadType == ImageView.LOAD_TYPE_CACHED_FULL)
      return true;

    // Prefetch if we have been going in the same direction for long enough.
    return sequenceLength >= 3;
  }

  var self = this;
  this.metadataProvider_.fetch(selectedItem.getUrl(), function(metadata){
     if (!selectedItem.isSelected()) return;
     self.client_.openImage(
         selectedItem.getIndex(), selectedItem.getUrl(), metadata, step,
         function(loadType) {
           if (!selectedItem.isSelected()) return;
           if (shouldPrefetch(loadType, step, self.sequenceLength_)) {
             self.requestPrefetch(step);
           }
           if (opt_callback) opt_callback();
         });
  });
};

Ribbon.prototype.requestPrefetch = function(direction) {
  if (this.items_.length < 2) return;

  var index = this.getNextSelectedIndex_(direction);
  var nextItemUrl = this.items_[index].getUrl();

  var selectedItem = this.getSelectedItem();
  this.metadataProvider_.fetch(nextItemUrl, function(metadata) {
    if (!selectedItem.isSelected()) return;
    this.client_.prefetchImage(index, nextItemUrl, metadata);
  }.bind(this));
};

Ribbon.ITEMS_COUNT = 5;

Ribbon.prototype.redraw = function() {
  // Never show a single thumbnail.
  if (this.items_.length == 1)
    return;

  var initThumbnail = function(index) {
    var item = this.items_[index];
    if (!item.hasThumbnail())
      this.metadataProvider_.fetch(item.getUrl(), item.setThumbnail.bind(item));
  }.bind(this);

  // TODO(dgozman): use margin instead of 2 here.
  var itemWidth = this.bar_.clientHeight - 2;
  var fullItems = Ribbon.ITEMS_COUNT;
  fullItems = Math.min(fullItems, this.items_.length);
  var right = Math.floor((fullItems - 1) / 2);

  var fullWidth = fullItems * itemWidth;
  this.bar_.style.width = fullWidth + 'px';

  var lastIndex = this.selectedIndex_ + right;
  lastIndex = Math.max(lastIndex, fullItems - 1);
  lastIndex = Math.min(lastIndex, this.items_.length - 1);
  var firstIndex = lastIndex - fullItems + 1;

  if (this.firstVisibleIndex_ == firstIndex &&
      this.lastVisibleIndex_ == lastIndex) {
    return;
  }

  if (this.lastVisibleIndex_ == -1) {
    this.firstVisibleIndex_ = firstIndex;
    this.lastVisibleIndex_ = lastIndex;
  }

  this.bar_.textContent = '';
  var startIndex = Math.min(firstIndex, this.firstVisibleIndex_);
  var toRemove = [];
  // All the items except the first one treated equally.
  for (var index = startIndex + 1;
       index <= Math.max(lastIndex, this.lastVisibleIndex_);
       ++index) {
    initThumbnail(index);
    var box = this.items_[index].getBox();
    box.style.marginLeft = '0';
    this.bar_.appendChild(box);
    if (index < firstIndex || index > lastIndex) {
      toRemove.push(index);
    }
  }

  var margin = itemWidth * Math.abs(firstIndex - this.firstVisibleIndex_);
  initThumbnail(startIndex);
  var startBox = this.items_[startIndex].getBox();
  if (startIndex == firstIndex) {
    // Sliding to the right.
    startBox.style.marginLeft = -margin + 'px';
    if (this.bar_.firstChild)
      this.bar_.insertBefore(startBox, this.bar_.firstChild);
    else
      this.bar_.appendChild(startBox);
    setTimeout(function() {
      startBox.style.marginLeft = '0';
    }, 0);
  } else {
    // Sliding to the left. Start item will become invisible and should be
    // removed afterwards.
    toRemove.push(startIndex);
    startBox.style.marginLeft = '0';
    if (this.bar_.firstChild)
      this.bar_.insertBefore(startBox, this.bar_.firstChild);
    else
      this.bar_.appendChild(startBox);
    setTimeout(function() {
      startBox.style.marginLeft = -margin + 'px';
    }, 0);
  }

  if (firstIndex > 0 && this.selectedIndex_ != firstIndex) {
    this.bar_.classList.add('fade-left');
  } else {
    this.bar_.classList.remove('fade-left');
  }

  if (lastIndex < this.items_.length - 1 && this.selectedIndex_ != lastIndex) {
    this.bar_.classList.add('fade-right');
  } else {
    this.bar_.classList.remove('fade-right');
  }

  this.firstVisibleIndex_ = firstIndex;
  this.lastVisibleIndex_ = lastIndex;

  setTimeout(function() {
    for (var i = 0; i < toRemove.length; i++) {
      var index = toRemove[i];
      if (index < this.firstVisibleIndex_ || index > this.lastVisibleIndex_) {
        var box = this.items_[index].getBox();
        if (box.parentNode == this.bar_)
          this.bar_.removeChild(box);
      }
    }
  }.bind(this), 200);
};

Ribbon.prototype.getNextSelectedIndex_ = function(direction) {
  var index = this.selectedIndex_ + (direction > 0 ? 1 : -1);
  if (index == -1) return this.items_.length - 1;
  if (index == this.items_.length) return 0;
  return index;
};

Ribbon.prototype.selectNext = function(direction, opt_callback) {
  this.select(this.getNextSelectedIndex_(direction), direction, opt_callback);
};

Ribbon.prototype.selectFirst = function() {
  this.select(0);
};

Ribbon.prototype.selectLast = function() {
  this.select(this.items_.length - 1);
};

/**
 * Start/stop the slide show. This is useful for performance debugging and
 * available only through a cryptic keyboard shortcut.
 */
Ribbon.prototype.toggleDebugSlideshow = function() {
  if (this.slideShowTimeout_) {
    clearInterval(this.slideShowTimeout_);
    this.slideShowTimeout_ = null;
  } else {
    var self = this;
    function nextSlide() {
      self.selectNext(1,
          function() { self.slideShowTimeout_ = setTimeout(nextSlide, 5000) });
    }
    nextSlide();
  }
};

Ribbon.Item = function(index, url, document, selectClosure) {
  this.index_ = index;
  this.url_ = url;

  this.box_ = document.createElement('div');
  this.box_.className = 'ribbon-image';
  this.box_.addEventListener('click', selectClosure);

  this.wrapper_ = document.createElement('div');
  this.wrapper_.className = 'image-wrapper';
  this.box_.appendChild(this.wrapper_);

  this.img_ = document.createElement('img');
  this.wrapper_.appendChild(this.img_);

  this.original_ = true;
  this.nameForSaving_ = null;
};

Ribbon.Item.prototype.getIndex = function () { return this.index_ };

Ribbon.Item.prototype.getBox = function () { return this.box_ };

Ribbon.Item.prototype.isOriginal = function () { return this.original_ };

Ribbon.Item.prototype.getUrl = function () { return this.url_ };
Ribbon.Item.prototype.setUrl = function (url) { this.url_ = url };

Ribbon.Item.prototype.getNameAfterSaving = function () {
  return this.nameForSaving_ || ImageUtil.getFullNameFromUrl(this.url_);
};

Ribbon.Item.prototype.hasNameForSaving = function() {
  return !!this.nameForSaving_;
};

Ribbon.Item.prototype.isSelected = function() {
  return this.box_.hasAttribute('selected');
};

Ribbon.Item.prototype.select = function(on) {
  ImageUtil.setAttribute(this.box_, 'selected', on);
};

Ribbon.Item.prototype.saveToFile = function(
    dirEntry, canvas, metadataEncoder, opt_callback) {
  ImageUtil.metrics.startInterval(ImageUtil.getMetricName('SaveTime'));

  var self = this;

  var name = this.getNameAfterSaving();
  this.original_ = false;
  this.nameForSaving_ = null;

  function onSuccess(url) {
    console.log('Saved from gallery', name);
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 1, 2);
    ImageUtil.metrics.recordInterval(ImageUtil.getMetricName('SaveTime'));
    self.setUrl(url);
    if (opt_callback) opt_callback(true);
  }

  function onError(error) {
    console.log('Error saving from gallery', name, error);
    ImageUtil.metrics.recordEnum(ImageUtil.getMetricName('SaveResult'), 0, 2);
    if (opt_callback) opt_callback(false);
  }

  function doSave(newFile, fileEntry) {
    fileEntry.createWriter(function(fileWriter) {
      function writeContent() {
        fileWriter.onwriteend = onSuccess.bind(null, fileEntry.toURL());
        fileWriter.write(ImageEncoder.getBlob(canvas, metadataEncoder));
      }
      fileWriter.onerror = function(error) {
        onError(error);
        // Disable all callbacks on the first error.
        fileWriter.onerror = null;
        fileWriter.onwriteend = null;
      };
      if (newFile) {
        writeContent();
      } else {
        fileWriter.onwriteend = writeContent;
        fileWriter.truncate(0);
      }
    }, onError);
  }

  function getFile(newFile) {
    dirEntry.getFile(name, {create: newFile, exclusive: newFile},
        doSave.bind(null, newFile), onError);
  }

  dirEntry.getFile(name, {create: false, exclusive: false},
      getFile.bind(null, false /* existing file */),
      getFile.bind(null, true /* create new file */));
};

// TODO: Localize?
Ribbon.Item.COPY_SIGNATURE = 'Edited';

Ribbon.Item.REGEXP_COPY_N =
    new RegExp('^' + Ribbon.Item.COPY_SIGNATURE + ' \\((\\d+)\\)( - .+)$');

Ribbon.Item.REGEXP_COPY_0 =
    new RegExp('^' + Ribbon.Item.COPY_SIGNATURE + '( - .+)$');

Ribbon.Item.prototype.createCopyName_ = function(dirEntry, metadata, callback) {
  var name = ImageUtil.getFullNameFromUrl(this.url_);

  // If the item represents a file created during the current Gallery session
  // we reuse it for subsequent saves instead of creating multiple copies.
  if (!this.original_)
    return name;

  var ext = '';
  var index = name.lastIndexOf('.');
  if (index != -1) {
    ext = name.substr(index);
    name = name.substr(0, index);
  }

  var mimeType = metadata.mimeType.toLowerCase();
  if (mimeType != 'image/jpeg') {
    // Chrome can natively encode only two formats: JPEG and PNG.
    // All non-JPEG images are saved in PNG, hence forcing the file extension.
    ext = '.png';
  }

  function tryNext(tries) {
    // All the names are used. Let's overwrite the last one.
    if (tries == 0) {
      setTimeout(callback, 0, name + ext);
      return;
    }

    // If the file name contains the copy signature add/advance the sequential
    // number.
    var matchN = Ribbon.Item.REGEXP_COPY_N.exec(name);
    var match0 = Ribbon.Item.REGEXP_COPY_0.exec(name);
    if (matchN && matchN[1] && matchN[2]) {
      var copyNumber = parseInt(matchN[1], 10) + 1;
      name = Ribbon.Item.COPY_SIGNATURE + ' (' + copyNumber + ')' + matchN[2];
    } else if (match0 && match0[1]) {
        name = Ribbon.Item.COPY_SIGNATURE + ' (1)' + match0[1];
    } else {
      name = Ribbon.Item.COPY_SIGNATURE + ' - ' + name;
    }

    dirEntry.getFile(name + ext, {create: false, exclusive: false},
        tryNext.bind(null, tries - 1),
        callback.bind(null, name + ext));
  }

  tryNext(10);
};

Ribbon.Item.prototype.setCopyName = function(dirEntry, metadata, opt_callback) {
  this.createCopyName_(dirEntry, metadata, function(name) {
    this.nameForSaving_ = name;
    if (opt_callback) opt_callback();
  }.bind(this));
};

Ribbon.Item.prototype.setOriginalName = function(dirEntry, opt_callback) {
  this.nameForSaving_ = null;
  if (opt_callback) opt_callback();
};

Ribbon.Item.prototype.setNameForSaving = function(newName) {
  this.nameForSaving_ = newName;
};

Ribbon.Item.prototype.hasThumbnail = function() {
  return this.img_.hasAttribute('src');
};

Ribbon.Item.prototype.setThumbnail = function(metadata) {
  var url;
  var transform;

  var mediaType = FileType.getMediaType(this.url_);

  if (metadata.thumbnailURL) {
    url = metadata.thumbnailURL;
    transform = metadata.thumbnailTransform;
  } else if (mediaType == 'image' &&
      FileType.canUseImageUrlForPreview(metadata)) {
    url = this.url_;
    transform = metadata.imageTransform;
  } else {
    url = FileType.getPreviewArt(mediaType);
  }

  function percent(ratio) { return Math.round(ratio * 100) + '%' }

  function resizeToFill(img, aspect) {
    if ((aspect > 1)) {
      img.style.height = percent(1);
      img.style.width = percent(aspect);
      img.style.marginLeft = percent((1 - aspect) / 2);
      img.style.marginTop = 0;
    } else {
      aspect = 1 / aspect;
      img.style.width = percent(1);
      img.style.height = percent(aspect);
      img.style.marginTop = percent((1 - aspect) / 2);
      img.style.marginLeft = 0;
    }
  }

  util.applyTransform(this.wrapper_, transform);

  var img = this.img_;

  if (metadata.width && metadata.height) {
    var aspect = metadata.width / metadata.height;
    if (transform && transform.rotate90) {
      aspect = 1 / aspect;
    }
    resizeToFill(img, aspect);
  } else {
    // No metadata available, loading the thumbnail first,
    // then adjust the size.
    img.style.maxWidth = '100%';
    img.style.maxHeight = '100%';
    img.onload = function (e) {
      var image = e.target;
      image.style.maxWidth = '';
      image.style.maxHeight = '';
      resizeToFill(image, image.width / image.height);
    }
  }

  img.onerror = function() {
    // Use the default icon if we could not fetch the correct one.
    util.applyTransform(this.wrapper_, null);
    img.src = FileType.getPreviewArt(mediaType);
  }.bind(this);

  img.src = url;
};

function ShareMode(editor, container, toolbar, shareActions,
                   onClick, actionCallback, displayStringFunction) {
  ImageEditor.Mode.call(this, 'share');

  this.message_ = null;

  var doc = container.ownerDocument;
  var button = doc.createElement('div');
  button.className = 'button share';
  button.textContent = displayStringFunction('share');
  button.addEventListener('click', onClick);
  toolbar.appendChild(button);

  this.bind(editor, button);

  this.menu_ = doc.createElement('div');
  this.menu_.className = 'share-menu';
  this.menu_.setAttribute('hidden', 'hidden');
  for (var index = 0; index < shareActions.length; index++) {
    var action = shareActions[index];
    var row = doc.createElement('div');
    var img = doc.createElement('img');
    img.src = action.iconUrl;
    row.appendChild(img);
    row.appendChild(doc.createTextNode(action.title));
    row.addEventListener('click', actionCallback.bind(null, action));
    this.menu_.appendChild(row);
  }
  var arrow = doc.createElement('div');
  arrow.className = 'bubble-point';
  this.menu_.appendChild(arrow);
  container.appendChild(this.menu_);
}

ShareMode.prototype = { __proto__: ImageEditor.Mode.prototype };

ShareMode.prototype.setUp = function() {
  ImageEditor.Mode.prototype.setUp.apply(this, arguments);
  ImageUtil.setAttribute(this.menu_, 'hidden', false);
  ImageUtil.setAttribute(this.button_, 'pressed', false);
};

ShareMode.prototype.cleanUpUI = function() {
  ImageEditor.Mode.prototype.cleanUpUI.apply(this, arguments);
  ImageUtil.setAttribute(this.menu_, 'hidden', true);
};
