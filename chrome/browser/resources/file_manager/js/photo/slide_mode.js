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
 * @param {function(string):string} displayStringFunction String formatting
 *     function.
 * @constructor
 */
function SlideMode(container, content, toolbar, prompt,
                   dataModel, selectionModel,
                   context, displayStringFunction) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.content = content;
  this.toolbar_ = toolbar;
  this.prompt_ = prompt;
  this.dataModel_ = dataModel;
  this.selectionModel_ = selectionModel;
  this.context_ = context;
  this.metadataCache_ = context.metadataCache;
  this.displayStringFunction_ = displayStringFunction;

  this.onSelectionBound_ = this.onSelection_.bind(this);
  this.onSpliceBound_ = this.onSplice_.bind(this);

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
  this.ribbon_ = new Ribbon(this.document_,
      this.metadataCache_, this.dataModel_, this.selectionModel_);
  this.ribbonSpacer_.appendChild(this.ribbon_);

  // Error indicator.
  var errorWrapper = util.createChild(this.container_, 'prompt-wrapper');
  errorWrapper.setAttribute('pos', 'center');

  this.errorBanner_ = util.createChild(errorWrapper, 'error-banner');

  util.createChild(this.container_, 'spinner');

  this.slideShowButton_ = util.createChild(this.toolbar_, 'button slideshow');
  this.slideShowButton_.title = this.displayStringFunction_('slideshow');
  this.slideShowButton_.addEventListener('click',
      this.toggleSlideshow_.bind(this, SlideMode.SLIDESHOW_INTERVAL_FIRST));

  // Editor.

  this.editButton_ = util.createChild(this.toolbar_, 'button edit');
  this.editButton_.title = this.displayStringFunction_('edit');
  this.editButton_.addEventListener('click', this.toggleEditor_.bind(this));

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
      new SwipeOverlay(this.selectNext.bind(this)));
};

/**
 * Load items, display the selected item.
 *
 * @param {function} opt_callback Callback.
 */
SlideMode.prototype.enter = function(opt_callback) {
  this.container_.setAttribute('mode', 'slide');

  this.sequenceDirection_ = 0;
  this.sequenceLength_ = 0;

  var loadDone = function() {
    this.active_ = true;

    this.selectionModel_.addEventListener('change', this.onSelectionBound_);
    this.dataModel_.addEventListener('splice', this.onSpliceBound_);

    this.ribbon_.enable();

    this.prefetchTimer_ = setTimeout(function() {
      this.prefetchTimer_ = null;
      this.requestPrefetch(1);   // Prefetch the next image.
    }.bind(this), 1000);
    if (opt_callback) opt_callback();
  }.bind(this);

  if (this.getItemCount_() == 0) {
    this.displayedIndex_ = -1;
    //TODO(kaznacheev) Show this message in the grid mode too.
    this.showErrorBanner_('NO_IMAGES');
    loadDone();
  } else {
    // Ensure valid single selection.
    // Note that the SlideMode object is not listening to selection change yet.
    this.select(Math.max(0, this.getSelectedIndex()));
    cr.dispatchSimpleEvent(this, 'namechange');  // Update name in the UI.
    this.displayedIndex_ = this.getSelectedIndex();

    var selectedItem = this.getSelectedItem();
    var selectedUrl = selectedItem.getUrl();
    // Show the selected item ASAP, then complete the initialization
    // (loading the ribbon thumbnails can take some time).
    this.metadataCache_.get(selectedUrl, Gallery.METADATA_TYPE,
        function(metadata) {
          this.loadItem_(selectedUrl, metadata, 0, loadDone);
        }.bind(this));

  }
};

/**
 * Leave the mode.
 * @param {function} opt_callback Callback.
 */
SlideMode.prototype.leave = function(opt_callback) {
  if (this.prefetchTimer_) {
    clearTimeout(this.prefetchTimer_);
    this.prefetchTimer_ = null;
  }

  var commitDone = function() {
      this.stopEditing_();
      this.stopSlideshow_();
      this.unloadImage_();
      this.selectionModel_.removeEventListener(
          'change', this.onSelectionBound_);
      this.dataModel_.removeEventListener('splice', this.onSpliceBound_);
      this.ribbon_.disable();
      this.active_ = false;
      if (opt_callback) opt_callback();
    }.bind(this);

  if (this.getItemCount_() == 0) {
    this.showErrorBanner_(false);
    commitDone();
  } else {
    this.commitItem_(commitDone);
  }
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
  return this.selectionModel_.selectedIndexes.length ?
      this.selectionModel_.selectedIndexes[0] :
      -1;
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
  this.selectionModel_.unselectAll();
  this.selectionModel_.setIndexSelected(index, true);
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
  var onMetadata = function(metadata) {
    if (selectedItem != this.getSelectedItem()) return;
    this.loadItem_(selectedItem.getUrl(), metadata, step,
        function(loadType) {
          if (selectedItem != this.getSelectedItem()) return;
          if (shouldPrefetch(loadType, step, this.sequenceLength_)) {
            this.requestPrefetch(step);
          }
          if (this.isSlideshowOn_())
            this.scheduleNextSlide_();
        }.bind(this));
  }.bind(this);
  this.metadataCache_.get(
      selectedItem.getUrl(), Gallery.METADATA_TYPE, onMetadata);
};

/**
 * Unload the current image.
 * @private
 */
SlideMode.prototype.unloadImage_ = function() {
  this.imageView_.unload();
  this.container_.removeAttribute('video');
};

/**
 * Data model 'splice' event handler.
 * @param {Event} event Event.
 * @private
 */
SlideMode.prototype.onSplice_ = function(event) {
  ImageUtil.setAttribute(this.arrowLeft_, 'active', this.getItemCount_() > 1);
  ImageUtil.setAttribute(this.arrowRight_, 'active', this.getItemCount_() > 1);

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
  var index = this.getSelectedIndex() + (direction > 0 ? 1 : -1);
  if (index == -1) return this.getItemCount_() - 1;
  if (index == this.getItemCount_()) return 0;
  return index;
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

    var times = SlideMode.OVERWRITE_BUBBLE_KEY in localStorage ?
        parseInt(localStorage[SlideMode.OVERWRITE_BUBBLE_KEY], 10) : 0;
    if (times < SlideMode.OVERWRITE_BUBBLE_MAX_TIMES) {
      this.bubble_.hidden = false;
      if (this.isEditing()) {
        localStorage[SlideMode.OVERWRITE_BUBBLE_KEY] = times + 1;
      }
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
  if (this.getItemCount_() <= 1) return;

  var index = this.getNextSelectedIndex_(direction);
  var nextItemUrl = this.getItem(index).getUrl();

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
      this.toggleEditor_();
      break;

    case 'U+001B':  // Escape
      if (!this.isEditing())
        return false;  // Not handled.
      this.toggleEditor_();
      break;

    case 'Ctrl-U+00DD':  // Ctrl+]. TODO(kaznacheev): Find a non-cryptic key.
      this.toggleSlideshow_(SlideMode.SLIDESHOW_INTERVAL_FIRST);
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
        this.ribbon_.updateThumbnail(newUrl, this.selectedImageMetadata_);
        cr.dispatchSimpleEvent(this, 'content');

        if (this.imageView_.getContentRevision() == 1) {  // First edit.
          // Lock the 'Overwrite original' checkbox for this item.
          ImageUtil.setAttribute(this.options_, 'saved', true);
          ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('Edit'));
        }

        if (oldUrl != newUrl) {
          this.displayedIndex_++;
          // This splice call will change the selection change event. SlideMode
          // will ignore it as the selected item is the same.
          // The ribbon will redraw while being obscured by the Editor toolbar,
          // so there is no need for nice animation here.
          this.dataModel_.splice(
              this.getSelectedIndex(), 0, new Gallery.Item(oldUrl));
        }
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
 * @return {boolean} True if the slideshow is on.
 * @private
 */
SlideMode.prototype.isSlideshowOn_ = function() {
  return this.container_.hasAttribute('slideshow');
};

/**
 * Stop the slideshow if it is on.
 * @private
 */
SlideMode.prototype.stopSlideshow_ = function() {
  if (this.isSlideshowOn_())
    this.toggleSlideshow_();
};

/**
 * Start/stop the slideshow.
 *
 * @param {number} opt_interval First interval in ms.
 * @param {Event} opt_event Event.
 * @private
 */

SlideMode.prototype.toggleSlideshow_ = function(opt_interval, opt_event) {
  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  if (!this.active_) {
    // Enter the slide mode. Show the first image for the full interval.
    this.enter(this.toggleSlideshow_.bind(this, SlideMode.SLIDESHOW_INTERVAL));
    return;
  }

  this.stopEditing_();
  ImageUtil.setAttribute(this.container_, 'slideshow', !this.isSlideshowOn_());
  ImageUtil.setAttribute(
      this.slideShowButton_, 'pressed', this.isSlideshowOn_());

  if (this.isSlideshowOn_()) {
    this.scheduleNextSlide_(opt_interval);
  } else {
    if (this.slideShowTimeout_) {
      clearInterval(this.slideShowTimeout_);
      this.slideShowTimeout_ = null;
    }
  }
};

/**
 * @param {number} opt_interval Slideshow interval in ms.
 * @private
 */
SlideMode.prototype.scheduleNextSlide_ = function(opt_interval) {
  if (this.slideShowTimeout_)
    clearTimeout(this.slideShowTimeout_);

  this.slideShowTimeout_ = setTimeout(function() {
        this.slideShowTimeout_ = null;
        this.selectNext(1);
      }.bind(this),
      opt_interval || SlideMode.SLIDESHOW_INTERVAL);
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
    this.toggleEditor_();
};

/**
 * Activate/deactivate editor.
 * @param {Event} opt_event Event.
 * @private
 */
SlideMode.prototype.toggleEditor_ = function(opt_event) {
  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  if (!this.active_) {
    this.enter(this.toggleEditor_.bind(this));
    return;
  }

  this.stopSlideshow_();
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
