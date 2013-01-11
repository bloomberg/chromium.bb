// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Slide mode displays a single image and has a set of controls to navigate
 * between the images and to edit an image.
 *
 * TODO(kaznacheev): Introduce a parameter object.
 *
 * @param {Element} container Main container element.
 * @param {Element} content Content container element.
 * @param {Element} toolbar Toolbar element.
 * @param {ImageEditor.Prompt} prompt Prompt.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @param {Object} context Context.
 * @param {function(function())} toggleMode Function to toggle the Gallery mode.
 * @param {function} onThumbnailError Thumbnail load error handler.
 * @param {function(string):string} displayStringFunction String formatting
 *     function.
 * @constructor
 */
function SlideMode(container, content, toolbar, prompt,
                   dataModel, selectionModel, context,
                   toggleMode, onThumbnailError, displayStringFunction) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.content = content;
  this.toolbar_ = toolbar;
  this.prompt_ = prompt;
  this.dataModel_ = dataModel;
  this.selectionModel_ = selectionModel;
  this.context_ = context;
  this.metadataCache_ = context.metadataCache;
  this.toggleMode_ = toggleMode;
  this.onThumbnailError_ = onThumbnailError;
  this.displayStringFunction_ = displayStringFunction;

  this.onSelectionBound_ = this.onSelection_.bind(this);
  this.onSpliceBound_ = this.onSplice_.bind(this);
  this.onContentBound_ = this.onContentChange_.bind(this);

  // Unique numeric key, incremented per each load attempt used to discard
  // old attempts. This can happen especially when changing selection fast or
  // Internet connection is slow.
  this.currentUniqueKey_ = 0;

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
 * @return {string} Mode name
 */
SlideMode.prototype.getName = function() { return 'slide' };

/**
 * Initialize the listeners.
 * @private
 */
SlideMode.prototype.initListeners_ = function() {
  window.addEventListener('resize', this.onResize_.bind(this), false);
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
  util.platform.getPreference(SlideMode.OVERWRITE_KEY, function(value) {
    // Out-of-the box default is 'true'
    this.overwriteOriginal_.checked =
        (typeof value != 'string' || value == 'true');
  }.bind(this));
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
      this.advanceManually.bind(this, -1));
  util.createChild(this.arrowLeft_);

  util.createChild(this.arrowBox_, 'arrow-spacer');

  this.arrowRight_ =
      util.createChild(this.arrowBox_, 'arrow right tool dimmable');
  this.arrowRight_.addEventListener('click',
      this.advanceManually.bind(this, 1));
  util.createChild(this.arrowRight_);

  this.ribbonSpacer_ = util.createChild(this.toolbar_, 'ribbon-spacer');
  this.ribbon_ = new Ribbon(this.document_,
      this.metadataCache_, this.dataModel_, this.selectionModel_,
      this.onThumbnailError_);
  this.ribbonSpacer_.appendChild(this.ribbon_);

  // Error indicator.
  var errorWrapper = util.createChild(this.container_, 'prompt-wrapper');
  errorWrapper.setAttribute('pos', 'center');

  this.errorBanner_ = util.createChild(errorWrapper, 'error-banner');

  util.createChild(this.container_, 'spinner');

  var slideShowButton = util.createChild(this.toolbar_,
      'button slideshow', 'button');
  slideShowButton.title = this.displayStringFunction_('slideshow');
  slideShowButton.addEventListener('click',
      this.startSlideshow.bind(this, SlideMode.SLIDESHOW_INTERVAL_FIRST));

  var slideShowToolbar =
      util.createChild(this.container_, 'tool slideshow-toolbar');
  util.createChild(slideShowToolbar, 'slideshow-play').
      addEventListener('click', this.toggleSlideshowPause_.bind(this));
  util.createChild(slideShowToolbar, 'slideshow-end').
      addEventListener('click', this.stopSlideshow_.bind(this));

  // Editor.

  var editButton_ = util.createChild(this.toolbar_, 'button edit', 'button');
  editButton_.title = this.displayStringFunction_('edit');
  editButton_.addEventListener('click', this.toggleEditor.bind(this));

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
      new SwipeOverlay(this.advanceManually.bind(this)));
};

/**
 * Load items, display the selected item.
 * @param {Rect} zoomFromRect Rectangle for zoom effect.
 * @param {function} displayCallback Called when the image is displayed.
 * @param {function} loadCallback Called when the image is displayed.
 */
SlideMode.prototype.enter = function(
    zoomFromRect, displayCallback, loadCallback) {
  this.sequenceDirection_ = 0;
  this.sequenceLength_ = 0;

  var loadDone = function(loadType, delay) {
    this.active_ = true;

    this.selectionModel_.addEventListener('change', this.onSelectionBound_);
    this.dataModel_.addEventListener('splice', this.onSpliceBound_);
    this.dataModel_.addEventListener('content', this.onContentBound_);

    ImageUtil.setAttribute(this.arrowBox_, 'active', this.getItemCount_() > 1);
    this.ribbon_.enable();

    // Wait 1000ms after the animation is done, then prefetch the next image.
    this.requestPrefetch(1, delay + 1000);

    if (loadCallback) loadCallback();
  }.bind(this);

  // The latest |leave| call might have left the image animating. Remove it.
  this.unloadImage_();

  if (this.getItemCount_() == 0) {
    this.displayedIndex_ = -1;
    //TODO(kaznacheev) Show this message in the grid mode too.
    this.showErrorBanner_('NO_IMAGES');
    loadDone();
  } else {
    // Remember the selection if it is empty or multiple. It will be restored
    // in |leave| if the user did not changing the selection manually.
    var currentSelection = this.selectionModel_.selectedIndexes;
    if (currentSelection.length == 1)
      this.savedSelection_ = null;
    else
      this.savedSelection_ = currentSelection;

    // Ensure valid single selection.
    // Note that the SlideMode object is not listening to selection change yet.
    this.select(Math.max(0, this.getSelectedIndex()));
    this.displayedIndex_ = this.getSelectedIndex();

    var selectedItem = this.getSelectedItem();
    var selectedUrl = selectedItem.getUrl();
    // Show the selected item ASAP, then complete the initialization
    // (loading the ribbon thumbnails can take some time).
    this.metadataCache_.get(selectedUrl, Gallery.METADATA_TYPE,
        function(metadata) {
          this.loadItem_(selectedUrl, metadata,
              zoomFromRect && this.imageView_.createZoomEffect(zoomFromRect),
              displayCallback, loadDone);
        }.bind(this));

  }
};

/**
 * Leave the mode.
 * @param {Rect} zoomToRect Rectangle for zoom effect.
 * @param {function} callback Called when the image is committed and
 *   the zoom-out animation has started.
 */
SlideMode.prototype.leave = function(zoomToRect, callback) {
  var commitDone = function() {
      this.stopEditing_();
      this.stopSlideshow_();
      ImageUtil.setAttribute(this.arrowBox_, 'active', false);
      this.selectionModel_.removeEventListener(
          'change', this.onSelectionBound_);
      this.dataModel_.removeEventListener('splice', this.onSpliceBound_);
      this.dataModel_.removeEventListener('content', this.onContentBound_);
      this.ribbon_.disable();
      this.active_ = false;
      if (this.savedSelection_)
        this.selectionModel_.selectedIndexes = this.savedSelection_;
      this.unloadImage_(zoomToRect);
      callback();
    }.bind(this);

  if (this.getItemCount_() == 0) {
    this.showErrorBanner_(false);
    commitDone();
  } else {
    this.commitItem_(commitDone);
  }
};


/**
 * Execute an action when the editor is not busy.
 *
 * @param {function} action Function to exectute.
 */
SlideMode.prototype.executeWhenReady = function(action) {
  this.editor_.executeWhenReady(action);
};

/**
 * @return {boolean} True if the mode has active tools (that should not fade).
 */
SlideMode.prototype.hasActiveTool = function() {
  return this.isEditing();
};

/**
 * @return {number} Item count.
 * @private
 */
SlideMode.prototype.getItemCount_ = function() {
  return this.dataModel_.length;
};

/**
 * @param {number} index Index.
 * @return {Gallery.Item} Item.
 */
SlideMode.prototype.getItem = function(index) {
  return this.dataModel_.item(index);
};

/**
 * @return {Gallery.Item} Selected index.
 */
SlideMode.prototype.getSelectedIndex = function() {
  return this.selectionModel_.selectedIndex;
};

/**
 * @return {Rect} Screen rectangle of the selected image.
 */
SlideMode.prototype.getSelectedImageRect = function() {
  if (this.getSelectedIndex() < 0)
    return null;
  else
    return this.viewport_.getScreenClipped();
};

/**
 * @return {Gallery.Item} Selected item
 */
SlideMode.prototype.getSelectedItem = function() {
  return this.getItem(this.getSelectedIndex());
};

/**
 * Selection change handler.
 *
 * Commits the current image and displays the newly selected image.
 * @private
 */
SlideMode.prototype.onSelection_ = function() {
  if (this.selectionModel_.selectedIndexes.length == 0)
    return;  // Temporary empty selection.

  // Forget the saved selection if the user changed the selection manually.
  if (!this.isSlideshowOn_())
    this.savedSelection_ = null;

  if (this.getSelectedIndex() == this.displayedIndex_)
    return;  // Do not reselect.

  this.commitItem_(this.loadSelectedItem_.bind(this));
};

/**
 * Change the selection.
 *
 * @param {number} index New selected index.
 * @param {number} opt_slideHint Slide animation direction (-1|1).
 */
SlideMode.prototype.select = function(index, opt_slideHint) {
  this.slideHint_ = opt_slideHint;
  this.selectionModel_.selectedIndex = index;
  this.selectionModel_.leadIndex = index;
};

/**
 * Load the selected item.
 *
 * @private
 */
SlideMode.prototype.loadSelectedItem_ = function() {
  var slideHint = this.slideHint_;
  this.slideHint_ = undefined;

  var index = this.getSelectedIndex();
  if (index == this.displayedIndex_)
    return;  // Do not reselect.

  var step = slideHint || (index - this.displayedIndex_);

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
    this.imageView_.prefetch(this.imageView_.contentID_);
  }

  this.displayedIndex_ = index;

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
  this.currentUniqueKey_++;
  var selectedUniqueKey = this.currentUniqueKey_;
  var onMetadata = function(metadata) {
    // Discard, since another load has been invoked after this one.
    if (selectedUniqueKey != this.currentUniqueKey_) return;
    this.loadItem_(selectedItem.getUrl(), metadata,
        new ImageView.Effect.Slide(step, this.isSlideshowPlaying_()),
        function() {} /* no displayCallback */,
        function(loadType, delay) {
          // Discard, since another load has been invoked after this one.
          if (selectedUniqueKey != this.currentUniqueKey_) return;
          if (shouldPrefetch(loadType, step, this.sequenceLength_)) {
            this.requestPrefetch(step, delay);
          }
          if (this.isSlideshowPlaying_())
            this.scheduleNextSlide_();
        }.bind(this));
  }.bind(this);
  this.metadataCache_.get(
      selectedItem.getUrl(), Gallery.METADATA_TYPE, onMetadata);
};

/**
 * Unload the current image.
 *
 * @param {Rect} zoomToRect Rectangle for zoom effect.
 * @private
 */
SlideMode.prototype.unloadImage_ = function(zoomToRect) {
  this.imageView_.unload(zoomToRect);
  this.container_.removeAttribute('video');
};

/**
 * Data model 'splice' event handler.
 * @param {Event} event Event.
 * @private
 */
SlideMode.prototype.onSplice_ = function(event) {
  ImageUtil.setAttribute(this.arrowBox_, 'active', this.getItemCount_() > 1);

  // Splice invalidates saved indices, drop the saved selection.
  this.savedSelection_ = null;

  if (event.removed.length != 1)
    return;

  // Delay the selection to let the ribbon splice handler work first.
  setTimeout(function() {
    if (event.index < this.dataModel_.length) {
      // There is the next item, select it.
      // The next item is now at the same index as the removed one, so we need
      // to correct displayIndex_ so that loadSelectedItem_ does not think
      // we are re-selecting the same item (and does right-to-left slide-in
      // animation).
      this.displayedIndex_ = event.index - 1;
      this.select(event.index);
    } else if (this.dataModel_.length) {
      // Removed item is the rightmost, but there are more items.
      this.select(event.index - 1);  // Select the new last index.
    } else {
      // No items left. Unload the image and show the banner.
      this.commitItem_(function() {
        this.unloadImage_();
        this.showErrorBanner_('NO_IMAGES');
      }.bind(this));
    }
  }.bind(this), 0);
};

/**
 * @param {number} direction -1 for left, 1 for right.
 * @return {number} Next index in the gived direction, with wrapping.
 * @private
 */
SlideMode.prototype.getNextSelectedIndex_ = function(direction) {
  function advance(index, limit) {
    index += (direction > 0 ? 1 : -1);
    if (index < 0)
      return limit - 1;
    if (index == limit)
      return 0;
    return index;
  }

  // If the saved selection is multiple the Slideshow should cycle through
  // the saved selection.
  if (this.isSlideshowOn_() &&
      this.savedSelection_ && this.savedSelection_.length > 1) {
    var pos = advance(this.savedSelection_.indexOf(this.getSelectedIndex()),
        this.savedSelection_.length);
    return this.savedSelection_[pos];
  } else {
    return advance(this.getSelectedIndex(), this.getItemCount_());
  }
};

/**
 * Advance the selection based on the pressed key ID.
 * @param {string} keyID Key identifier.
 */
SlideMode.prototype.advanceWithKeyboard = function(keyID) {
  this.advanceManually(keyID == 'Up' || keyID == 'Left' ? -1 : 1);
};

/**
 * Advance the selection as a result of a user action (as opposed to an
 * automatic change in the slideshow mode).
 * @param {number} direction -1 for left, 1 for right.
 */
SlideMode.prototype.advanceManually = function(direction) {
  if (this.isSlideshowPlaying_()) {
    this.pauseSlideshow_();
    cr.dispatchSimpleEvent(this, 'useraction');
  }
  this.selectNext(direction);
};

/**
 * Select the next item.
 * @param {number} direction -1 for left, 1 for right.
 */
SlideMode.prototype.selectNext = function(direction) {
  this.select(this.getNextSelectedIndex_(direction), direction);
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
  this.select(this.getItemCount_() - 1);
};

// Loading/unloading

/**
 * Load and display an item.
 *
 * @param {string} url Item url.
 * @param {Object} metadata Item metadata.
 * @param {Object} effect Transition effect object.
 * @param {function} displayCallback Called when the image is displayed
 *     (which can happen before the image load due to caching).
 * @param {function} loadCallback Called when the image is fully loaded.
 * @private
 */
SlideMode.prototype.loadItem_ = function(
    url, metadata, effect, displayCallback, loadCallback) {
  this.selectedImageMetadata_ = ImageUtil.deepCopy(metadata);

  this.showSpinner_(true);

  var loadDone = function(loadType, delay, error) {
    var video = this.isShowingVideo_();
    ImageUtil.setAttribute(this.container_, 'video', video);

    this.showSpinner_(false);
    if (loadType == ImageView.LOAD_TYPE_ERROR) {
      // if we have a specific error, then display it
      if (error) {
        this.showErrorBanner_(error);
      } else {
        // otherwise try to infer general error
        this.showErrorBanner_(video ? 'VIDEO_ERROR' : 'IMAGE_ERROR');
      }
    } else if (loadType == ImageView.LOAD_TYPE_OFFLINE) {
      this.showErrorBanner_(video ? 'VIDEO_OFFLINE' : 'IMAGE_OFFLINE');
    }

    if (video) {
      // The editor toolbar does not make sense for video, hide it.
      this.stopEditing_();
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

    // For once edited image, disallow the 'overwrite' setting change.
    ImageUtil.setAttribute(this.options_, 'saved',
        !this.getSelectedItem().isOriginal());

    util.platform.getPreference(SlideMode.OVERWRITE_BUBBLE_KEY,
        function(value) {
          var times = typeof value == 'string' ? parseInt(value, 10) : 0;
          if (times < SlideMode.OVERWRITE_BUBBLE_MAX_TIMES) {
            this.bubble_.hidden = false;
            if (this.isEditing()) {
              util.platform.setPreference(
                  SlideMode.OVERWRITE_BUBBLE_KEY, times + 1);
            }
          }
        }.bind(this));

    loadCallback(loadType, delay);
  }.bind(this);

  var displayDone = function() {
    cr.dispatchSimpleEvent(this, 'image-displayed');
    displayCallback();
  }.bind(this);

  this.editor_.openSession(url, metadata, effect,
      this.saveCurrentImage_.bind(this), displayDone, loadDone);
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
 * @param {number} delay Delay in ms. Used to prevent the CPU-heavy image
 *   loading from disrupting the animation that might be still in progress.
 */
SlideMode.prototype.requestPrefetch = function(direction, delay) {
  if (this.getItemCount_() <= 1) return;

  var index = this.getNextSelectedIndex_(direction);
  var nextItemUrl = this.getItem(index).getUrl();
  this.imageView_.prefetch(nextItemUrl, delay);
};

// Event handlers.

/**
 * Unload handler, to be called from the top frame.
 * @param {boolean} exiting True if the app is exiting.
 */
SlideMode.prototype.onUnload = function(exiting) {
  if (this.isShowingVideo_() && this.mediaControls_.isPlaying()) {
    this.mediaControls_.savePosition(exiting);
  }
};

/**
 * beforeunload handler, to be called from the top frame.
 * @return {string} Message to show if there are unsaved changes.
 */
SlideMode.prototype.onBeforeUnload = function() {
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
  var keyID = util.getKeyModifiers(event) + event.keyIdentifier;

  if (this.isSlideshowOn_()) {
    switch (keyID) {
      case 'U+001B':  // Escape exits the slideshow.
        this.stopSlideshow_(event);
        break;

      case 'U+0020':  // Space pauses/resumes the slideshow.
        this.toggleSlideshowPause_();
        break;

      case 'Up':
      case 'Down':
      case 'Left':
      case 'Right':
        this.advanceWithKeyboard(keyID);
        break;
    }
    return true;  // Consume all keystrokes in the slideshow mode.
  }

  if (this.isEditing() && this.editor_.onKeyDown(event))
    return true;

  switch (keyID) {
    case 'U+0020':  // Space toggles the video playback.
      if (this.isShowingVideo_()) {
        this.mediaControls_.togglePlayStateWithFeedback();
      }
      break;

    case 'U+0045':  // 'e' toggles the editor
      this.toggleEditor(event);
      break;

    case 'U+001B':  // Escape
      if (!this.isEditing())
        return false;  // Not handled.
      this.toggleEditor(event);
      break;

    case 'Home':
      this.selectFirst();
      break;
    case 'End':
      this.selectLast();
      break;
    case 'Up':
    case 'Down':
    case 'Left':
    case 'Right':
      this.advanceWithKeyboard(keyID);
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

/**
 * Update thumbnails.
 */
SlideMode.prototype.updateThumbnails = function() {
  this.ribbon_.reset();
  if (this.active_)
    this.ribbon_.redraw();
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

        var e = new cr.Event('content');
        e.item = item;
        e.oldUrl = oldUrl;
        e.metadata = this.selectedImageMetadata_;
        this.dataModel_.dispatchEvent(e);

        // Allow changing the 'Overwrite original' setting only if the user
        // used Undo to restore the original image AND it is not a copy.
        // Otherwise lock the setting in its current state.
        var mayChangeOverwrite = !this.editor_.canUndo() && item.isOriginal();
        ImageUtil.setAttribute(this.options_, 'saved', !mayChangeOverwrite);

        if (this.imageView_.getContentRevision() == 1) {  // First edit.
          ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('Edit'));
        }

        if (oldUrl != item.getUrl()) {
          this.dataModel_.splice(
              this.getSelectedIndex(), 0, new Gallery.Item(oldUrl));
          // The ribbon will ignore the splice above and redraw after the
          // select call below (while being obscured by the Editor toolbar,
          // so there is no need for nice animation here).
          // SlideMode will ignore the selection change as the displayed item
          // index has not changed.
          this.select(++this.displayedIndex_);
        }
        callback();
        cr.dispatchSimpleEvent(this, 'image-saved');
      }.bind(this));
};

/**
 * Update caches when the selected item url has changed.
 *
 * @param {Event} event Event.
 * @private
 */
SlideMode.prototype.onContentChange_ = function(event) {
  var newUrl = event.item.getUrl();
  if (newUrl != event.oldUrl) {
    this.imageView_.changeUrl(newUrl);
  }
  this.metadataCache_.clear(event.oldUrl, Gallery.METADATA_TYPE);
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
   return this.overwriteOriginal_.checked;
};

/**
 * 'Overwrite original' checkbox handler.
 * @param {Event} event Event.
 * @private
 */
SlideMode.prototype.onOverwriteOriginalClick_ = function(event) {
  util.platform.setPreference(SlideMode.OVERWRITE_KEY, event.target.checked);
};

/**
 * Overwrite info bubble close handler.
 * @private
 */
SlideMode.prototype.onCloseBubble_ = function() {
  this.bubble_.hidden = true;
  util.platform.setPreference(SlideMode.OVERWRITE_BUBBLE_KEY,
      SlideMode.OVERWRITE_BUBBLE_MAX_TIMES);
};

// Slideshow

/**
 * Slideshow interval in ms.
 */
SlideMode.SLIDESHOW_INTERVAL = 5000;

/**
 * First slideshow interval in ms. It should be shorter so that the user
 * is not guessing whether the button worked.
 */
SlideMode.SLIDESHOW_INTERVAL_FIRST = 1000;

/**
 * Empirically determined duration of the fullscreen toggle animation.
 */
SlideMode.FULLSCREEN_TOGGLE_DELAY = 500;

/**
 * @return {boolean} True if the slideshow is on.
 * @private
 */
SlideMode.prototype.isSlideshowOn_ = function() {
  return this.container_.hasAttribute('slideshow');
};

/**
 * Start the slideshow.
 * @param {number} opt_interval First interval in ms.
 * @param {Event} opt_event Event.
 */
SlideMode.prototype.startSlideshow = function(opt_interval, opt_event) {
  // Set the attribute early to prevent the toolbar from flashing when
  // the slideshow is being started from the mosaic view.
  this.container_.setAttribute('slideshow', 'playing');

  if (this.active_) {
    this.stopEditing_();
  } else {
    // We are in the Mosaic mode. Toggle the mode but remember to return.
    this.leaveAfterSlideshow_ = true;
    this.toggleMode_(this.startSlideshow.bind(
        this, SlideMode.SLIDESHOW_INTERVAL, opt_event));
    return;
  }

  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  this.fullscreenBeforeSlideshow_ = false;
  util.platform.getWindowStatus(function(info) {
    if (info.state == 'maximized') {
      this.resumeSlideshow_(opt_interval);
      return;  // Do not go fullscreen if already maximized.
    }
    // Wait until the zoom animation from the mosaic mode is done.
    setTimeout(function() {
      Gallery.getFileBrowserPrivate().isFullscreen(function(fullscreen) {
        this.fullscreenBeforeSlideshow_ = fullscreen;
        if (!fullscreen) {
          Gallery.toggleFullscreen();
          opt_interval = (opt_interval || SlideMode.SLIDESHOW_INTERVAL) +
              SlideMode.FULLSCREEN_TOGGLE_DELAY;
        }
        this.resumeSlideshow_(opt_interval);
      }.bind(this));
    }.bind(this), ImageView.ZOOM_ANIMATION_DURATION);
  }.bind(this));
};

/**
 * Stop the slideshow.
 * @param {Event} opt_event Event.
 * @private
   */
SlideMode.prototype.stopSlideshow_ = function(opt_event) {
  if (!this.isSlideshowOn_())
    return;

  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  this.pauseSlideshow_();
  this.container_.removeAttribute('slideshow');
  Gallery.getFileBrowserPrivate().isFullscreen(function(fullscreen) {
    // Do not restore fullscreen if we exited fullscreen while in slideshow.
    var toggleModeDelay = 0;
    if (!this.fullscreenBeforeSlideshow_ && fullscreen) {
      Gallery.toggleFullscreen();
      toggleModeDelay = SlideMode.FULLSCREEN_TOGGLE_DELAY;
    }
    if (this.leaveAfterSlideshow_) {
      this.leaveAfterSlideshow_ = false;
      setTimeout(this.toggleMode_.bind(this), toggleModeDelay);
    }
  }.bind(this));
};

/**
 * @return {boolean} True if the slideshow is playing (not paused).
 * @private
 */
SlideMode.prototype.isSlideshowPlaying_ = function() {
  return this.container_.getAttribute('slideshow') == 'playing';
};

/**
 * Pause/resume the slideshow.
 * @private
 */
SlideMode.prototype.toggleSlideshowPause_ = function() {
  cr.dispatchSimpleEvent(this, 'useraction');  // Show the tools.
  if (this.isSlideshowPlaying_()) {
    this.pauseSlideshow_();
  } else {
    this.resumeSlideshow_(SlideMode.SLIDESHOW_INTERVAL_FIRST);
  }
};

/**
 * @param {number} opt_interval Slideshow interval in ms.
 * @private
 */
SlideMode.prototype.scheduleNextSlide_ = function(opt_interval) {
  console.assert(this.isSlideshowPlaying_(), 'Inconsistent slideshow state');

  if (this.slideShowTimeout_)
    clearTimeout(this.slideShowTimeout_);

  this.slideShowTimeout_ = setTimeout(function() {
        this.slideShowTimeout_ = null;
        this.selectNext(1);
      }.bind(this),
      opt_interval || SlideMode.SLIDESHOW_INTERVAL);
};

/**
 * Resume the slideshow.
 * @param {number} opt_interval Slideshow interval in ms.
 * @private
 */
SlideMode.prototype.resumeSlideshow_ = function(opt_interval) {
  this.container_.setAttribute('slideshow', 'playing');
  this.scheduleNextSlide_(opt_interval);
};

/**
 * Pause the slideshow.
 * @private
 */
SlideMode.prototype.pauseSlideshow_ = function() {
  this.container_.setAttribute('slideshow', 'paused');
  if (this.slideShowTimeout_) {
    clearTimeout(this.slideShowTimeout_);
    this.slideShowTimeout_ = null;
  }
};

/**
 * @return {boolean} True if the editor is active.
 */
SlideMode.prototype.isEditing = function() {
  return this.container_.hasAttribute('editing');
};

/**
 * Stop editing.
 * @private
 */
SlideMode.prototype.stopEditing_ = function() {
  if (this.isEditing())
    this.toggleEditor();
};

/**
 * Activate/deactivate editor.
 * @param {Event} opt_event Event.
 */
SlideMode.prototype.toggleEditor = function(opt_event) {
  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  if (!this.active_) {
    this.toggleMode_(this.toggleEditor.bind(this));
    return;
  }

  this.stopSlideshow_();
  if (!this.isEditing() && this.isShowingVideo_())
    return;  // No editing for videos.

  ImageUtil.setAttribute(this.container_, 'editing', !this.isEditing());

  if (this.isEditing()) { // isEditing has just been flipped to a new value.
    if (this.context_.readonlyDirName) {
      this.editor_.getPrompt().showAt(
          'top', 'readonly_warning', 0, this.context_.readonlyDirName);
    }
  } else {
    this.editor_.getPrompt().hide();
    this.editor_.leaveModeGently();
  }
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
