// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Slide mode displays a single image and has a set of controls to navigate
 * between the images and to edit an image.
 *
 * @param {Element} container Container element.
 * @param {Element} toolbar Toolbar element.
 * @param {ImageEditor.Prompt} prompt Prompt.
 * @param {Object} context Context.
 * @param {function(string):string} displayStringFunction String formatting
 *     function.
 * @constructor
 */
function SlideMode(container, toolbar, prompt, context, displayStringFunction) {
  this.container_ = container;
  this.toolbar_ = toolbar;
  this.document_ = container.ownerDocument;
  this.prompt_ = prompt;
  this.context_ = context;
  this.metadataCache_ = context.metadataCache;
  this.displayStringFunction_ = displayStringFunction;

  this.initListeners_();
  this.initDom_();
}

/**
 * SlideMode extends cr.EventTarget.
 */
SlideMode.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * List of available editor modes.
 * @type {Array.<ImageEditor.Mode>}
 */
SlideMode.editorModes = [
  new ImageEditor.Mode.InstantAutofix(),
  new ImageEditor.Mode.Crop(),
  new ImageEditor.Mode.Exposure(),
  new ImageEditor.Mode.OneClick('rotate_left', new Command.Rotate(-1)),
  new ImageEditor.Mode.OneClick('rotate_right', new Command.Rotate(1))
];

/**
 * Initialize the listeners.
 * @private
 */
SlideMode.prototype.initListeners_ = function() {
  var win = this.document_.defaultView;

  this.onBeforeUnloadBound_ = this.onBeforeUnload_.bind(this);
  win.top.addEventListener('beforeunload', this.onBeforeUnloadBound_);

  win.addEventListener('unload', this.onUnload_.bind(this));

  // We need to listen to the top window 'unload' and 'beforeunload' because
  // the Gallery iframe does not get notified if the tab is closed.
  this.onTopUnloadBound_ = this.onTopUnload_.bind(this);
  win.top.addEventListener('unload', this.onTopUnloadBound_);

  win.addEventListener('resize', this.onResize_.bind(this), false);
};

/**
 * Initialize the UI.
 * @private
 */
SlideMode.prototype.initDom_ = function() {
  // Container for displayed image or video.
  this.imageContainer_ = util.createChild(
      this.document_.querySelector('.content'), 'image-container');
  this.imageContainer_.addEventListener('click', this.onClick_.bind(this));

  // Overwrite options and info bubble.
  this.options_ = util.createChild(
      this.toolbar_.querySelector('.filename-spacer'), 'options');

  this.savedLabel_ = util.createChild(this.options_, 'saved');
  this.savedLabel_.textContent = this.displayStringFunction_('saved');

  var overwriteOriginalBox =
      util.createChild(this.options_, 'overwrite-original');

  this.overwriteOriginal_ = util.createChild(
      overwriteOriginalBox, 'common white', 'input');
  this.overwriteOriginal_.type = 'checkbox';
  this.overwriteOriginal_.id = 'overwrite-checkbox';
  this.overwriteOriginal_.checked = this.shouldOverwriteOriginal_();
  this.overwriteOriginal_.addEventListener('click',
      this.onOverwriteOriginalClick_.bind(this));

  var overwriteLabel = util.createChild(overwriteOriginalBox, '', 'label');
  overwriteLabel.textContent =
      this.displayStringFunction_('overwrite_original');
  overwriteLabel.setAttribute('for', 'overwrite-checkbox');

  this.bubble_ = util.createChild(this.toolbar_, 'bubble');
  this.bubble_.hidden = true;

  var bubbleContent = util.createChild(this.bubble_);
  bubbleContent.innerHTML = this.displayStringFunction_('overwrite_bubble');

  util.createChild(this.bubble_, 'pointer bottom', 'span');

  var bubbleClose = util.createChild(this.bubble_, 'close-x');
  bubbleClose.addEventListener('click', this.onCloseBubble_.bind(this));

  // Video player controls.
  this.mediaSpacer_ =
      util.createChild(this.container_, 'video-controls-spacer');
  this.mediaToolbar_ = util.createChild(this.mediaSpacer_, 'tool');
  this.mediaControls_ = new VideoControls(
      this.mediaToolbar_,
      this.showErrorBanner_.bind(this, 'VIDEO_ERROR'),
      Gallery.toggleFullscreen,
      this.container_);

  // Ribbon and related controls.
  this.arrowBox_ = util.createChild(this.container_, 'arrow-box');

  this.arrowLeft_ =
      util.createChild(this.arrowBox_, 'arrow left tool dimmable');
  this.arrowLeft_.addEventListener('click',
      this.selectNext.bind(this, -1, null));
  util.createChild(this.arrowLeft_);

  util.createChild(this.arrowBox_, 'arrow-spacer');

  this.arrowRight_ =
      util.createChild(this.arrowBox_, 'arrow right tool dimmable');
  this.arrowRight_.addEventListener('click',
      this.selectNext.bind(this, 1, null));
  util.createChild(this.arrowRight_);

  this.ribbonSpacer_ = util.createChild(this.toolbar_, 'ribbon-spacer');

  // Error indicator.
  var errorWrapper = util.createChild(this.container_, 'prompt-wrapper');
  errorWrapper.setAttribute('pos', 'center');

  this.errorBanner_ = util.createChild(errorWrapper, 'error-banner');

  util.createChild(this.container_, 'spinner');


  this.editButton_ = util.createChild(this.toolbar_, 'button edit');
  this.editButton_.textContent = this.displayStringFunction_('edit');
  this.editButton_.addEventListener('click', this.onEdit_.bind(this));

  // Editor toolbar.

  this.editBar_ = util.createChild(this.toolbar_, 'edit-bar');
  this.editBarMain_ = util.createChild(this.editBar_, 'edit-main');

  this.editBarMode_ = util.createChild(this.container_, 'edit-modal');
  this.editBarModeWrapper_ = util.createChild(
      this.editBarMode_, 'edit-modal-wrapper');
  this.editBarModeWrapper_.hidden = true;

  // Objects supporting image display and editing.
  this.viewport_ = new Viewport();

  this.imageView_ = new ImageView(
      this.imageContainer_,
      this.viewport_,
      this.metadataCache_);

  this.imageView_.addContentCallback(this.onImageContentChanged_.bind(this));

  this.editor_ = new ImageEditor(
      this.viewport_,
      this.imageView_,
      this.prompt_,
      {
        root: this.container_,
        image: this.imageContainer_,
        toolbar: this.editBarMain_,
        mode: this.editBarModeWrapper_
      },
      SlideMode.editorModes,
      this.displayStringFunction_);

  this.editor_.getBuffer().addOverlay(
      new SwipeOverlay(this.selectNext.bind(this)));
};

/**
 * Load items, display the selected item.
 *
 * @param {Array.<Gallery.Item>} items Array of items.
 * @param {number} selectedIndex Selected index.
 * @param {function} callback Callback.
 */
SlideMode.prototype.load = function(items, selectedIndex, callback) {
  var selectedItem = items[selectedIndex];
  if (!selectedItem) {
    this.showErrorBanner_('IMAGE_ERROR');
    return;
  }

  var loadDone = function() {
    this.items_ = items;
    this.setSelectedIndex_(selectedIndex);
    this.setupNavigation_();
    setTimeout(this.requestPrefetch.bind(this, 1 /* Next item */), 1000);
    callback();
  }.bind(this);

  var selectedUrl = selectedItem.getUrl();
  // Show the selected item ASAP, then complete the initialization
  // (loading the ribbon thumbnails can take some time).
  this.metadataCache_.get(selectedUrl, Gallery.METADATA_TYPE,
      function(metadata) {
        this.loadItem_(selectedUrl, metadata, 0, loadDone);
      }.bind(this));
};

/**
 * Setup navigation controls.
 * @private
 */
SlideMode.prototype.setupNavigation_ = function() {
  this.sequenceDirection_ = 0;
  this.sequenceLength_ = 0;

  ImageUtil.setAttribute(this.arrowLeft_, 'active', this.items_.length > 1);
  ImageUtil.setAttribute(this.arrowRight_, 'active', this.items_.length > 1);

  this.ribbon_ = new Ribbon(
      this.document_, this.metadataCache_, this.select.bind(this));
  this.ribbonSpacer_.appendChild(this.ribbon_);
  this.ribbon_.update(this.items_, this.selectedIndex_);
};

/**
 * @return {Gallery.Item} Selected item
 */
SlideMode.prototype.getSelectedItem = function() {
  return this.items_ && this.items_[this.selectedIndex_];
};

/**
 * Change the selection.
 *
 * Commits the current image and changes the selection.
 *
 * @param {number} index New selected index.
 * @param {number} opt_slideDir Slide animation direction (-1|0|1).
 *   Derived from the new and the old selected indices if omitted.
 * @param {function} opt_callback Callback.
 */
SlideMode.prototype.select = function(index, opt_slideDir, opt_callback) {
  if (!this.items_)
    return;  // Not fully initialized, still loading the first image.

  if (index == this.selectedIndex_)
    return;  // Do not reselect.

  this.commitItem_(
      this.doSelect_.bind(this, index, opt_slideDir, opt_callback));
};

/**
 * Set the new selected index value.
 * @param {number} index New selected index.
 * @private
 */
SlideMode.prototype.setSelectedIndex_ = function(index) {
  this.selectedIndex_ = index;
  cr.dispatchSimpleEvent(this, 'selection');

  // For once edited image, disallow the 'overwrite' setting change.
  ImageUtil.setAttribute(this.options_, 'saved',
      !this.getSelectedItem().isOriginal());
};

/**
 * Perform the actual selection change.
 *
 * @param {number} index New selected index.
 * @param {number} opt_slideDir Slide animation direction (-1|0|1).
 *   Derived from the new and the old selected indices if omitted.
 * @param {function} opt_callback Callback.
 * @private
 */
SlideMode.prototype.doSelect_ = function(index, opt_slideDir, opt_callback) {
  if (index == this.selectedIndex_)
    return;  // Do not reselect

  var step = opt_slideDir != undefined ?
      opt_slideDir :
      (index - this.selectedIndex_);

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
    this.editor_.prefetchImage(this.getSelectedItem().getUrl());
  }

  this.setSelectedIndex_(index);

  if (this.ribbon_)
    this.ribbon_.update(this.items_, this.selectedIndex_);

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

  var selectedItem = this.getSelectedItem();
  var onMetadata = function(metadata) {
    if (selectedItem != this.getSelectedItem()) return;
    this.loadItem_(selectedItem.getUrl(), metadata, step,
        function(loadType) {
          if (selectedItem != this.getSelectedItem()) return;
          if (shouldPrefetch(loadType, step, this.sequenceLength_)) {
            this.requestPrefetch(step);
          }
          if (opt_callback) opt_callback();
        }.bind(this));
  }.bind(this);
  this.metadataCache_.get(
      selectedItem.getUrl(), Gallery.METADATA_TYPE, onMetadata);
};

/**
 * @param {number} direction -1 for left, 1 for right.
 * @return {number} Next index in the gived direction, with wrapping.
 * @private
 */
SlideMode.prototype.getNextSelectedIndex_ = function(direction) {
  var index = this.selectedIndex_ + (direction > 0 ? 1 : -1);
  if (index == -1) return this.items_.length - 1;
  if (index == this.items_.length) return 0;
  return index;
};

/**
 * Select the next item.
 * @param {number} direction -1 for left, 1 for right.
 * @param {function} opt_callback Callback.
 */
SlideMode.prototype.selectNext = function(direction, opt_callback) {
  this.select(this.getNextSelectedIndex_(direction), direction, opt_callback);
};

/**
 * Select the first item.
 */
SlideMode.prototype.selectFirst = function() {
  this.select(0);
};

/**
 * Select the last item.
 */
SlideMode.prototype.selectLast = function() {
  this.select(this.items_.length - 1);
};

// Loading/unloading

/**
 * Load and display an item.
 *
 * @param {string} url Item url.
 * @param {Object} metadata Item metadata.
 * @param {number} slide Slide animation direction (-1|0|1).
 * @param {function} callback Callback.
 * @private
 */
SlideMode.prototype.loadItem_ = function(url, metadata, slide, callback) {
  this.selectedImageMetadata_ = ImageUtil.deepCopy(metadata);

  this.showSpinner_(true);

  var loadDone = function(loadType) {
    var video = this.isShowingVideo_();
    ImageUtil.setAttribute(this.container_, 'video', video);

    this.showSpinner_(false);
    if (loadType == ImageView.LOAD_TYPE_ERROR) {
      this.showErrorBanner_(video ? 'VIDEO_ERROR' : 'IMAGE_ERROR');
    } else if (loadType == ImageView.LOAD_TYPE_OFFLINE) {
      this.showErrorBanner_(video ? 'VIDEO_OFFLINE' : 'IMAGE_OFFLINE');
    }

    if (video) {
      if (this.isEditing()) {
        // The editor toolbar does not make sense for video, hide it.
        this.onEdit_();
      }
      this.mediaControls_.attachMedia(this.imageView_.getVideo());
      //TODO(kaznacheev): Add metrics for video playback.
    } else {
      ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('View'));

      function toMillions(number) { return Math.round(number / (1000 * 1000)) }

      ImageUtil.metrics.recordSmallCount(ImageUtil.getMetricName('Size.MB'),
          toMillions(metadata.filesystem.size));

      var canvas = this.imageView_.getCanvas();
      ImageUtil.metrics.recordSmallCount(ImageUtil.getMetricName('Size.MPix'),
          toMillions(canvas.width * canvas.height));

      var extIndex = url.lastIndexOf('.');
      var ext = extIndex < 0 ? '' : url.substr(extIndex + 1).toLowerCase();
      if (ext == 'jpeg') ext = 'jpg';
      ImageUtil.metrics.recordEnum(
          ImageUtil.getMetricName('FileType'), ext, ImageUtil.FILE_TYPES);
    }

    callback(loadType);
  }.bind(this);

  this.editor_.openSession(
      url, metadata, slide, this.saveCurrentImage_.bind(this), loadDone);
};

/**
 * Commit changes to the current item and reset all messages/indicators.
 *
 * @param {function} callback Callback.
 * @private
 */
SlideMode.prototype.commitItem_ = function(callback) {
  this.showSpinner_(false);
  this.showErrorBanner_(false);
  this.editor_.getPrompt().hide();
  if (this.isShowingVideo_()) {
    this.mediaControls_.pause();
    this.mediaControls_.detachMedia();
  }
  this.editor_.closeSession(callback);
};

/**
 * Request a prefetch for the next image.
 *
 * @param {number} direction -1 or 1.
 */
SlideMode.prototype.requestPrefetch = function(direction) {
  if (this.items_.length <= 1) return;

  var index = this.getNextSelectedIndex_(direction);
  var nextItemUrl = this.items_[index].getUrl();

  var selectedItem = this.getSelectedItem();
  this.metadataCache_.get(nextItemUrl, Gallery.METADATA_TYPE,
      function(metadata) {
        if (selectedItem != this.getSelectedItem()) return;
        this.editor_.prefetchImage(nextItemUrl);
      }.bind(this));
};

// Event handlers.

/**
 * Unload handler.
 * @private
 */
SlideMode.prototype.onUnload_ = function() {
  this.saveVideoPosition_();
  window.top.removeEventListener('beforeunload', this.onBeforeUnloadBound_);
  window.top.removeEventListener('unload', this.onTopUnloadBound_);
};

/**
 * Top window unload handler.
 * @private
 */
SlideMode.prototype.onTopUnload_ = function() {
  this.saveVideoPosition_();
};

/**
 * Top window beforeunload handler.
 * @return {string} Message to show if there are unsaved changes.
 * @private
 */
SlideMode.prototype.onBeforeUnload_ = function() {
  if (this.editor_.isBusy())
    return this.displayStringFunction_('unsaved_changes');
  return null;
};

/**
 * Click handler.
 * @private
 */
SlideMode.prototype.onClick_ = function() {
  if (this.isShowingVideo_())
    this.mediaControls_.togglePlayStateWithFeedback();
};

/**
 * Keydown handler.
 *
 * @param {Event} event Event.
 * @return {boolean} True if handled.
 */
SlideMode.prototype.onKeyDown = function(event) {
  if (this.isEditing() && this.editor_.onKeyDown(event))
    return true;

  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+0020':  // Space toggles the video playback.
      if (this.isShowingVideo_()) {
        this.mediaControls_.togglePlayStateWithFeedback();
      }
      break;

    case 'U+0045':  // 'e' toggles the editor
      this.onEdit_();
      break;

    case 'U+001B':  // Escape
      if (!this.isEditing())
        return false;  // Not handled.
      this.onEdit_();
      break;

    case 'Ctrl-U+00DD':  // Ctrl+] (cryptic on purpose).
      this.toggleSlideshow_();
      break;

    case 'Home':
      this.selectFirst();
      break;
    case 'End':
      this.selectLast();
      break;
    case 'Left':
      this.selectNext(-1);
      break;
    case 'Right':
      this.selectNext(1);
      break;

    default: return false;
  }

  return true;
};

/**
 * Resize handler.
 * @private
 */
SlideMode.prototype.onResize_ = function() {
  this.viewport_.sizeByFrameAndFit(this.container_);
  this.viewport_.repaint();
};

// Saving

/**
 * Save the current image to a file.
 *
 * @param {function} callback Callback.
 * @private
 */
SlideMode.prototype.saveCurrentImage_ = function(callback) {
  var item = this.getSelectedItem();
  var oldUrl = item.getUrl();
  var canvas = this.imageView_.getCanvas();

  this.showSpinner_(true);
  var metadataEncoder = ImageEncoder.encodeMetadata(
      this.selectedImageMetadata_.media, canvas, 1 /* quality */);

  this.selectedImageMetadata_ = ContentProvider.ConvertContentMetadata(
      metadataEncoder.getMetadata(), this.selectedImageMetadata_);

  item.saveToFile(
      this.context_.saveDirEntry,
      this.shouldOverwriteOriginal_(),
      canvas,
      metadataEncoder,
      function(success) {
        // TODO(kaznacheev): Implement write error handling.
        // Until then pretend that the save succeeded.
        this.showSpinner_(false);
        this.flashSavedLabel_();
        var newUrl = item.getUrl();
        this.updateSelectedUrl_(oldUrl, newUrl);
        this.ribbon_.updateThumbnail(
            this.selectedIndex_, newUrl, this.selectedImageMetadata_);
        callback();
      }.bind(this));
};

/**
 * Update caches when the selected item url has changed.
 *
 * @param {string} oldUrl Old url.
 * @param {string} newUrl New url.
 * @private
 */
SlideMode.prototype.updateSelectedUrl_ = function(oldUrl, newUrl) {
  this.metadataCache_.clear(oldUrl, Gallery.METADATA_TYPE);

  if (oldUrl == newUrl)
    return;

  this.imageView_.changeUrl(newUrl);
  this.ribbon_.remapCache(oldUrl, newUrl);

  // Let the gallery know that the selected item url has changed.
  cr.dispatchSimpleEvent(this, 'selection');
};

/**
 * Flash 'Saved' label briefly to indicate that the image has been saved.
 * @private
 */
SlideMode.prototype.flashSavedLabel_ = function() {
  var setLabelHighlighted =
      ImageUtil.setAttribute.bind(null, this.savedLabel_, 'highlighted');
  setTimeout(setLabelHighlighted.bind(null, true), 0);
  setTimeout(setLabelHighlighted.bind(null, false), 300);
};

/**
 * Local storage key for the 'Overwrite original' setting.
 * @type {string}
 */
SlideMode.OVERWRITE_KEY = 'gallery-overwrite-original';

/**
 * Local storage key for the number of times that
 * the overwrite info bubble has been displayed.
 * @type {string}
 */
SlideMode.OVERWRITE_BUBBLE_KEY = 'gallery-overwrite-bubble';

/**
 * Max number that the overwrite info bubble is shown.
 * @type {number}
 */
SlideMode.OVERWRITE_BUBBLE_MAX_TIMES = 5;

/**
 * @return {boolean} True if 'Overwrite original' is set.
 * @private
 */
SlideMode.prototype.shouldOverwriteOriginal_ = function() {
  return SlideMode.OVERWRITE_KEY in localStorage &&
      (localStorage[SlideMode.OVERWRITE_KEY] == 'true');
};

/**
 * 'Overwrite original' checkbox handler.
 * @param {Event} event Event.
 * @private
 */
SlideMode.prototype.onOverwriteOriginalClick_ = function(event) {
  localStorage['gallery-overwrite-original'] = event.target.checked;
};

/**
 * Overwrite info bubble close handler.
 * @private
 */
SlideMode.prototype.onCloseBubble_ = function() {
  this.bubble_.hidden = true;
  localStorage[SlideMode.OVERWRITE_BUBBLE_KEY] =
      SlideMode.OVERWRITE_BUBBLE_MAX_TIMES;
};

/**
 * Callback called when the image is edited.
 * @private
 */
SlideMode.prototype.onImageContentChanged_ = function() {
  var revision = this.imageView_.getContentRevision();
  if (revision == 0) {
    // Just loaded.
    var times = SlideMode.OVERWRITE_BUBBLE_KEY in localStorage ?
        parseInt(localStorage[SlideMode.OVERWRITE_BUBBLE_KEY], 10) : 0;
    if (times < SlideMode.OVERWRITE_BUBBLE_MAX_TIMES) {
      this.bubble_.hidden = false;
      if (this.isEditing()) {
        localStorage[SlideMode.OVERWRITE_BUBBLE_KEY] = times + 1;
      }
    }
  }

  if (revision == 1) {
    // First edit.
    ImageUtil.setAttribute(this.options_, 'saved', true);
    ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('Edit'));
  }
};

// Misc

/**
 * Start/stop the slide show.
 * @private
 */
SlideMode.prototype.toggleSlideshow_ = function() {
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

/**
 * @return {boolean} True if the editor is active.
 */
SlideMode.prototype.isEditing = function() {
  return this.container_.hasAttribute('editing');
};

/**
 * Activate/deactivate editor.
 * @private
 */
SlideMode.prototype.onEdit_ = function() {
  if (!this.isEditing() && this.isShowingVideo_())
    return;  // No editing for videos.

  ImageUtil.setAttribute(this.container_, 'editing', !this.isEditing());

  if (this.isEditing()) { // isEditing_ has just been flipped to a new value.
    if (this.context_.readonlyDirName) {
      this.editor_.getPrompt().showAt(
          'top', 'readonly_warning', 0, this.context_.readonlyDirName);
    }
  } else {
    this.editor_.getPrompt().hide();
  }

  ImageUtil.setAttribute(this.editButton_, 'pressed', this.isEditing());

  cr.dispatchSimpleEvent(this, 'edit');
};

/**
 * Display the error banner.
 * @param {string} message Message.
 * @private
 */
SlideMode.prototype.showErrorBanner_ = function(message) {
  if (message) {
    this.errorBanner_.textContent = this.displayStringFunction_(message);
  }
  ImageUtil.setAttribute(this.container_, 'error', !!message);
};

/**
 * Show/hide the busy spinner.
 *
 * @param {boolean} on True if show, false if hide.
 * @private
 */
SlideMode.prototype.showSpinner_ = function(on) {
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

/**
 * @return {boolean} True if the current item is a video.
 * @private
 */
SlideMode.prototype.isShowingVideo_ = function() {
  return !!this.imageView_.getVideo();
};

/**
 * Save the current video position.
 * @private
 */
SlideMode.prototype.saveVideoPosition_ = function() {
  if (this.isShowingVideo_() && this.mediaControls_.isPlaying()) {
    this.mediaControls_.savePosition();
  }
};

/**
 * Overlay that handles swipe gestures. Changes to the next or previous file.
 * @param {function(number)} callback A callback accepting the swipe direction
 *    (1 means left, -1 right).
 * @constructor
 * @implements {ImageBuffer.Overlay}
 */
function SwipeOverlay(callback) {
  this.callback_ = callback;
}

/**
 * Inherit ImageBuffer.Overlay.
 */
SwipeOverlay.prototype.__proto__ = ImageBuffer.Overlay.prototype;

/**
 * @param {number} x X pointer position.
 * @param {number} y Y pointer position.
 * @param {boolean} touch True if dragging caused by touch.
 * @return {function} The closure to call on drag.
 */
SwipeOverlay.prototype.getDragHandler = function(x, y, touch) {
  if (!touch)
    return null;
  var origin = x;
  var done = false;
  return function(x, y) {
    if (!done && origin - x > SwipeOverlay.SWIPE_THRESHOLD) {
      this.callback_(1);
      done = true;
    } else if (!done && x - origin > SwipeOverlay.SWIPE_THRESHOLD) {
      this.callback_(-1);
      done = true;
    }
  }.bind(this);
};

/**
 * If the user touched the image and moved the finger more than SWIPE_THRESHOLD
 * horizontally it's considered as a swipe gesture (change the current image).
 */
SwipeOverlay.SWIPE_THRESHOLD = 100;
