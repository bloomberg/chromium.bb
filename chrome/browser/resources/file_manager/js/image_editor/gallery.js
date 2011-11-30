// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Base class that Ribbon uses to display photos.
 */

function RibbonClient() {}

RibbonClient.prototype.prefetchImage = function(id, content, metadata) {};

RibbonClient.prototype.openImage = function(id, content, metadata, direction) {
};

RibbonClient.prototype.closeImage = function(item) {};

/**
 * Image gallery for viewing and editing image files.
 *
 * @param {HTMLDivElement} container
 * @param {function} closeCallback
 * @param {MetadataProvider} metadataProvider
 * @param {Array.<Object>} shareActions
 */
function Gallery(container, closeCallback, metadataProvider, shareActions,
    displayStringFunction) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.closeCallback_ = closeCallback;
  this.metadataProvider_ = metadataProvider;

  this.displayStringFunction_ = function(id) {
    return displayStringFunction('GALLERY_' + id.toUpperCase());
  };

  this.onFadeTimeoutBound_ = this.onFadeTimeout_.bind(this);
  this.fadeTimeoutId_ = null;
  this.mouseOverTool_ = false;

  this.initDom_(shareActions);
}

Gallery.prototype = { __proto__: RibbonClient.prototype };

Gallery.open = function(parentDirEntry, items, selectedItem,
   closeCallback, metadataProvider, shareActions, displayStringFunction) {
  var container = document.querySelector('.gallery');
  ImageUtil.removeChildren(container);
  var gallery = new Gallery(container, closeCallback, metadataProvider,
      shareActions, displayStringFunction);
  gallery.load(parentDirEntry, items, selectedItem);
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

Gallery.prototype.initDom_ = function(shareActions) {
  var doc = this.document_;

  // Clean up after the previous instance of Gallery.
  this.container_.removeAttribute('editing');
  this.container_.removeAttribute('tools');
  if (window.galleryKeyDown) {
    doc.body.removeEventListener('keydown', window.galleryKeyDown);
  }
  if (window.galleryMouseMove) {
    doc.body.removeEventListener('mousemove', window.galleryMouseMove);
  }

  window.galleryKeyDown = this.onKeyDown_.bind(this);
  doc.body.addEventListener('keydown', window.galleryKeyDown);

  window.galleryMouseMove = this.onMouseMove_.bind(this);
  doc.body.addEventListener('mousemove', window.galleryMouseMove);

  this.closeButton_ = doc.createElement('div');
  this.closeButton_.className = 'close tool dimmable';
  this.closeButton_.appendChild(doc.createElement('div'));
  this.closeButton_.addEventListener('click', this.onClose_.bind(this));
  this.container_.appendChild(this.closeButton_);

  this.imageContainer_ = doc.createElement('div');
  this.imageContainer_.className = 'image-container';
  this.container_.appendChild(this.imageContainer_);

  this.toolbar_ = doc.createElement('div');
  this.toolbar_.className = 'toolbar tool dimmable';
  this.container_.appendChild(this.toolbar_);

  this.ribbonSpacer_ = doc.createElement('div');
  this.ribbonSpacer_.className = 'ribbon-spacer';
  this.toolbar_.appendChild(this.ribbonSpacer_);

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

  this.ribbon_ = new Ribbon(this.ribbonSpacer_,
      this, this.metadataProvider_, this.arrowLeft_, this.arrowRight_);

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

  this.editor_ = new ImageEditor(
      this.container_,
      this.imageContainer_,
      this.editBarMain_,
      this.editBarModeWrapper_,
      Gallery.editorModes,
      this.displayStringFunction_);

  this.imageView_ = this.editor_.getImageView();

  this.editor_.trackWindow(doc.defaultView);

  if (shareActions.length > 0) {
    this.shareMode_ = new ShareMode(
        this.container_, this.toolbar_, shareActions,
        this.onShare_.bind(this), this.onActionExecute_.bind(this),
        this.displayStringFunction_);

  } else {
    this.shareMode_ = null;
  }
};

Gallery.prototype.load = function(parentDirEntry, items, selectedItem) {
  this.parentDirEntry_ = parentDirEntry;

  var urls = [];
  var selectedIndex = -1;

  // Convert canvas and blob items to blob urls.
  for (var i = 0; i != items.length; i++) {
    var item = items[i];
    var selected = (item == selectedItem);

    if (item.constructor.name == 'HTMLCanvasElement') {
      item = ImageEncoder.getBlob(item,
          ImageEncoder.encodeMetadata({mimeType: 'image/jpeg'}, item), 1);
    }
    if (item.constructor.name == 'Blob' ||
        item.constructor.name == 'File') {
      item = window.webkitURL.createObjectURL(item);
    }
    if (typeof item == 'string') {
      if (selected) selectedIndex = urls.length;
      urls.push(item);
    } else {
      console.error('Unsupported image type');
    }
  }

  if (urls.length == 0)
    throw new Error('Cannot open the gallery for 0 items');

  if (selectedIndex == -1)
    throw new Error('Cannot find selected item');

  var self = this;

  function initRibbon() {
    self.ribbon_.load(urls, selectedIndex);
    // Flash the ribbon briefly to let the user know it is there.
    self.cancelFading_();
    self.initiateFading_(Gallery.FIRST_FADE_TIMEOUT);
  }

  var selectedURL = urls[selectedIndex];
  // Initialize the ribbon only after the selected image is fully loaded.
  this.metadataProvider_.fetch(selectedURL, function (metadata) {
    self.editor_.openSession(
        selectedIndex, selectedURL, metadata, 0, initRibbon);
  });
};

Gallery.prototype.saveItem_ = function(item, callback, canvas, modified) {
  if (modified) {
    item.save(
        this.parentDirEntry_, this.metadataProvider_, canvas, callback);
  } else {
    if (callback) callback();
  }
};

Gallery.prototype.saveChanges_ = function(opt_callback) {
  this.editor_.requestImage(
      this.saveItem_.bind(this, this.ribbon_.getSelectedItem(), opt_callback));
};

Gallery.prototype.onActionExecute_ = function(action) {
  var url = this.ribbon_.getSelectedItem().getUrl();
  // saveChanges_ makes the editor leave the mode and close the sharing menu.
  this.saveChanges_(action.execute.bind(action, [url]));
};

Gallery.prototype.onClose_ = function() {
  // TODO: handle write errors gracefully (suggest retry or saving elsewhere).
  this.saveChanges_(this.closeCallback_);
};

Gallery.prototype.prefetchImage = function(id, content, metadata) {
  this.editor_.prefetchImage(id, content, metadata);
};

Gallery.prototype.openImage = function(id, content, metadata, slide, callback) {
  this.editor_.openSession(id, content, metadata, slide, callback);
};

Gallery.prototype.closeImage = function(item) {
  this.editor_.closeSession(this.saveItem_.bind(this, item, null));
};

Gallery.prototype.isEditing_ = function() {
  return this.container_.hasAttribute('editing');
};

Gallery.prototype.onEdit_ = function() {
  ImageUtil.setAttribute(this.container_, 'editing', !this.isEditing_());

  // isEditing_ has just been flipped to a new value.
  if (this.isEditing_()) {
    this.cancelFading_();
  } else if (!this.isSharing_()) {
    var item = this.ribbon_.getSelectedItem();
    this.editor_.requestImage(item.updateThumbnail.bind(item));
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

  switch (event.keyIdentifier) {
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
      this.onEdit_();
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

    case 'U+00DD':
      if (event.ctrlKey)  // Ctrl+] (cryptic on purpose).
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
  if (this.isEditing_() || this.isSharing_()) return;
  this.container_.removeAttribute('tools');
};

Gallery.prototype.initiateFading_ = function(opt_timeout) {
  if (this.mouseOverTool_ || this.isEditing_() || this.isSharing_())
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

  this.arrowLeft_ = arrowLeft;
  this.arrowLeft_.
      addEventListener('click', this.selectNext.bind(this, -1, null));

  this.arrowRight_ = arrowRight;
  this.arrowRight_.
      addEventListener('click', this.selectNext.bind(this, 1, null));

  this.fadeLeft_ = this.document_.createElement('div');
  this.fadeLeft_.className = 'fade left';
  this.container_.appendChild(this.fadeLeft_);

  this.bars_ = [];
  for (var i = 0; i < 2; i++) {
    var bar = this.document_.createElement('div');
    bar.className = 'ribbon';
    this.container_.appendChild(bar);
    this.bars_.push(bar);
  }
  this.activeBarIndex_ = 0;
  ImageUtil.setAttribute(this.bars_[0], 'inactive', false);
  ImageUtil.setAttribute(this.bars_[1], 'inactive', true);

  this.fadeRight_ = this.document_.createElement('div');
  this.fadeRight_.className = 'fade right';
  this.container_.appendChild(this.fadeRight_);
}

Ribbon.PAGING_SINGLE_ITEM_DELAY = 20;
Ribbon.PAGING_ANIMATION_DURATION = 200;

Ribbon.prototype.getSelectedItem = function () {
  return this.items_[this.selectedIndex_];
};

Ribbon.prototype.clear = function() {
  this.bars_[0].textContent = '';
  this.bars_[1].textContent = '';
  this.activeBarIndex_ = 0;
  ImageUtil.setAttribute(this.bars_[0], 'inactive', false);
  ImageUtil.setAttribute(this.bars_[1], 'inactive', true);
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
  this.metadataProvider_.fetch(url, item.setMetadata.bind(item));
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

  var oldSelectedItem = this.getSelectedItem();
  oldSelectedItem.select(false);
  this.client_.closeImage(oldSelectedItem);

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
    this.client_.prefetchImage(oldSelectedItem.getIndex(),
        oldSelectedItem.getContent(), oldSelectedItem.getMetadata());
  }

  this.selectedIndex_ = index;

  var selectedItem = this.getSelectedItem();
  selectedItem.select(true);
  this.redraw();

  var self = this;
  selectedItem.fetchMetadata(this.metadataProvider_, function(metadata){
     if (!selectedItem.isSelected()) return;
     self.client_.openImage(
         selectedItem.getIndex(), selectedItem.getContent(), metadata, step,
         function(loadedInstantly) {
           if (!selectedItem.isSelected()) return;
           if (Math.abs(step) != 1) return;
           if (loadedInstantly || (self.sequenceLength_ >= 3)) {
             // We can always afford to prefetch if the previous load was
             // instant. Even if it was not we should start prefetching
             // if we have been going in the same direction for long enough.
             self.requestPrefetch(step);
           }
           if (opt_callback) opt_callback();
         });
  });
};

Ribbon.prototype.requestPrefetch = function(direction) {
  if (this.items_.length < 2) return;

  var index = this.getNextSelectedIndex_(direction);

  var selectedItem = this.getSelectedItem();
  var self = this;
  var item = this.items_[index];
  item.fetchMetadata(this.metadataProvider_, function(metadata) {
    if (!selectedItem.isSelected()) return;
    self.client_.prefetchImage(index, item.getContent(), metadata);
  });
};

Ribbon.prototype.updateControls_ = function() {
  ImageUtil.setAttribute(this.fadeLeft_, 'active', this.firstVisibleIndex_ > 0);
  ImageUtil.setAttribute(this.fadeRight_, 'active',
      this.lastVisibleIndex_ + 1 < this.items_.length);
};

Ribbon.prototype.redraw = function() {
  // The thumbnails are square.
  var itemWidth = this.bars_[0].parentNode.clientHeight;
  var width = this.bars_[0].parentNode.clientWidth;

  var fullItems = Math.floor(width / itemWidth);

  var fullWidth = fullItems * itemWidth;

  this.bars_[0].style.width = fullWidth + 'px';
  this.bars_[1].style.width = fullWidth + 'px';

  // Position the shadows over the first and the last visible thumbnails.
  this.fadeLeft_.style.left = 0;
  this.fadeRight_.style.right = (width - fullWidth) + 'px';

  fullItems = Math.min(fullItems, this.items_.length);
  var firstIndex = this.firstVisibleIndex_;
  var lastIndex = firstIndex + fullItems - 1;

  if (this.selectedIndex_ <= firstIndex && firstIndex > 0) {
    firstIndex = Math.max(0, this.selectedIndex_ - (fullItems >> 1));
    lastIndex = firstIndex + fullItems - 1;
  }

  if (this.selectedIndex_ >= lastIndex && lastIndex < this.items_.length - 1) {
    lastIndex = Math.min(this.items_.length - 1,
                         this.selectedIndex_ + (fullItems >> 1));
    firstIndex = lastIndex - fullItems + 1;
  }

  this.updateControls_();

  if (this.firstVisibleIndex_ == firstIndex &&
      this.lastVisibleIndex_ == lastIndex) {
    return;
  }

  var activeBar = this.bars_[this.activeBarIndex_];
  var children = activeBar.childNodes;
  var totalDuration = children.length * Ribbon.PAGING_SINGLE_ITEM_DELAY +
      Ribbon.PAGING_ANIMATION_DURATION;

  var direction = 1;
  var delay = 0;
  if (firstIndex < this.firstVisibleIndex_) {
    direction = -1;
    delay = (children.length - 1) * Ribbon.PAGING_SINGLE_ITEM_DELAY;
  }
  for (var index = 0; index < children.length; ++index) {
    setTimeout(
        ImageUtil.setAttribute.bind(null, children[index], 'inactive', true),
        delay);
    delay += direction * Ribbon.PAGING_SINGLE_ITEM_DELAY;
  }

  // Place inactive bar below active after animation is finished.
  setTimeout(
      ImageUtil.setAttribute.bind(null, activeBar, 'inactive', true),
      totalDuration);

  this.firstVisibleIndex_ = firstIndex;
  this.lastVisibleIndex_ = lastIndex;

  this.activeBarIndex_ = 1 - this.activeBarIndex_;
  activeBar = this.bars_[this.activeBarIndex_];
  activeBar.textContent = '';
  for (var index = firstIndex; index <= lastIndex; ++index) {
    var box = this.items_[index].getBox(this.activeBarIndex_);
    ImageUtil.setAttribute(box, 'inactive', false);
    activeBar.appendChild(box);
  }

  // Place active bar above inactive after animation is finished.
  setTimeout(
      ImageUtil.setAttribute.bind(null, activeBar, 'inactive', false),
      totalDuration);
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

  this.boxes_ = [];
  this.wrappers_ = [];
  this.imgs_ = [];

  for (var i = 0; i < 2; i++) {
    var box = document.createElement('div');
    box.className = 'ribbon-image';
    box.addEventListener('click', selectClosure);

    var wrapper = document.createElement('div');
    wrapper.className = 'image-wrapper';
    box.appendChild(wrapper);

    var img = document.createElement('img');
    wrapper.appendChild(img);

    this.boxes_.push(box);
    this.wrappers_.push(wrapper);
    this.imgs_.push(img);
  }

  this.original_ = true;
};

Ribbon.Item.prototype.getIndex = function () { return this.index_ };

Ribbon.Item.prototype.getBox = function (index) { return this.boxes_[index] };

Ribbon.Item.prototype.isOriginal = function () { return this.original_ };

Ribbon.Item.prototype.getUrl = function () { return this.url_ };

Ribbon.Item.prototype.isSelected = function() {
  return this.boxes_[0].hasAttribute('selected');
};

Ribbon.Item.prototype.select = function(on) {
  for (var i = 0; i < 2; i++) {
    ImageUtil.setAttribute(this.boxes_[i], 'selected', on);
  }
};

Ribbon.Item.prototype.updateThumbnail = function(canvas) {
  if (this.canvas_)
    return;  // The image is being saved, the thumbnail is already up-to-date.

  var metadataEncoder =
      ImageEncoder.encodeMetadata(this.getMetadata(), canvas, 1);
  this.setMetadata(metadataEncoder.getMetadata());
};

Ribbon.Item.prototype.save = function(
    dirEntry, metadataProvider, canvas, opt_callback) {
  var metadataEncoder =
      ImageEncoder.encodeMetadata(this.getMetadata(), canvas, 1);

  this.overrideContent(canvas, metadataEncoder.getMetadata());

  var self = this;

  if (!dirEntry) {  // Happens only in gallery_demo.js
    self.onSaveSuccess(
        window.webkitURL.createObjectURL(
            ImageEncoder.getBlob(canvas, metadataEncoder)));
    if (opt_callback) opt_callback();
    return;
  }

  var newFile = this.isOriginal();
  var name = this.getCopyName();

  function onSuccess(url) {
    console.log('Saved from gallery', name);
    // Force the metadata provider to reread the metadata from the file.
    metadataProvider.reset(url);
    self.onSaveSuccess(url);
    if (opt_callback) opt_callback();
  }

  function onError(error) {
    console.log('Error saving from gallery', name, error);
    self.onSaveError(error);
    if (opt_callback) opt_callback();
  }

  dirEntry.getFile(
      name, {create: newFile, exclusive: newFile}, function(fileEntry) {
        fileEntry.createWriter(function(fileWriter) {
          function writeContent() {
            fileWriter.onwriteend = onSuccess.bind(null, fileEntry.toURL());
            fileWriter.write(ImageEncoder.getBlob(canvas, metadataEncoder));
          }
          fileWriter.onerror = onError;
          if (newFile) {
            writeContent();
          } else {
            fileWriter.onwriteend = writeContent;
            fileWriter.truncate(0);
          }
        },
    onError);
  }, onError);
};

// TODO: Localize?
Ribbon.Item.COPY_SIGNATURE = '_Edited_';

Ribbon.Item.prototype.getCopyName = function () {
  // When saving a modified image we never overwrite the original file (the one
  // that existed prior to opening the Gallery. Instead we save to a file named
  // <original-name>_Edited_<date-stamp>.<original extension>.

  var name = this.url_.substr(this.url_.lastIndexOf('/') + 1);

  // If the item represents a file created during the current Gallery session
  // we reuse it for subsequent saves instead of creating multiple copies.
  if (!this.original_)
    return name;

  this.original_ = false;

  var ext = '';
  var index = name.lastIndexOf('.');
  if (index != -1) {
    ext = name.substr(index);
    name = name.substr(0, index);
  }
  var signaturePos = name.indexOf(Ribbon.Item.COPY_SIGNATURE);
  if (signaturePos >= 0) {
    // The file is likely to be a copy created during a previous session.
    // Replace the signature instead of appending a new one.
    name = name.substr(0, signaturePos);
  }

  var mimeType = this.metadata_.mimeType.toLowerCase();
  if (mimeType != 'image/jpeg') {
    // Chrome can natively encode only two formats: JPEG and PNG.
    // All non-JPEG images are saved in PNG, hence forcing the file extension.
    if (mimeType == 'image/png') {
      ext = '.png';
    } else {
      // All non-JPEG images get 'image/png' mimeType (see
      // ImageEncoder.MetadataEncoder constructor).
      // This code can be reached only if someone has added a metadata parser
      // for a format other than JPEG or PNG. The message below is to remind
      // that one must also come up with the way to encode the image data.
      console.error('Image encoding for ' + mimeType + ' is not supported');
    }
  }

  function twoDigits(n) { return (n < 10 ? '0' : '' ) + n }

  var now = new Date();

  // Datestamp the copy with YYYYMMDD_HHMMSS (similar to what many cameras do)
  return name +
      Ribbon.Item.COPY_SIGNATURE +
      now.getFullYear() +
      twoDigits(now.getMonth() + 1) +
      twoDigits(now.getDate()) +
      '_' +
      twoDigits(now.getHours()) +
      twoDigits(now.getMinutes()) +
      twoDigits(now.getSeconds()) +
      ext;
};

// The url and metadata stored in the item are not valid while the modified
// image is being saved. Use the results of the latest edit instead.

Ribbon.Item.prototype.overrideContent = function(canvas, metadata) {
  this.canvas_ = canvas;
  this.backupMetadata_ = this.metadata_;
  this.setMetadata(metadata);
};

Ribbon.Item.prototype.getContent = function () {
  return this.canvas_ || this.url_;
};

Ribbon.Item.prototype.getMetadata = function () {
  return this.metadata_;
};

Ribbon.Item.prototype.fetchMetadata = function (metadataProvider, callback) {
  if (this.metadata_) {
    setTimeout(callback.bind(null, this.metadata_), 0);
  } else {
    metadataProvider.fetch(this.getUrl(), callback);
  }
};

Ribbon.Item.prototype.onSaveSuccess = function(url) {
  this.url_ = url;
  delete this.backupMetadata_;
  delete this.canvas_;
};

Ribbon.Item.prototype.onSaveError = function(error) {
  // TODO(kaznacheev): notify the user that the file write failed and
  // suggest ways to rescue the modified image (retry/save elsewhere).
  // For now - just drop the modified content and revert the thumbnail.
  this.setMetadata(this.backupMetadata_);
  delete this.backupMetadata_;
  delete this.canvas_;
};

Ribbon.MAX_THUMBNAIL_PIXEL_COUNT = 1 << 20; // 1 MPix

Ribbon.PLACEHOLDER_ICON_URL = '../../images/filetype_large_image.png';

Ribbon.Item.prototype.setMetadata = function(metadata) {
  this.metadata_ = metadata;

  var url;
  var transform;

  if (metadata.thumbnailURL) {
    url = metadata.thumbnailURL;
    transform = metadata.thumbnailTransform;
  } else if (metadata.width && metadata.height &&
      (metadata.width * metadata.height < Ribbon.MAX_THUMBNAIL_PIXEL_COUNT)){
    // TODO(kaznacheev): We only have image dimensions for JPEG at the moment.
    // Need to add simple metadata parsers for all supported formats.
    // Another way would be getting the file size from FileManager.
    url = this.url_;
    transform = metadata.imageTransform;
  } else {
    url = Ribbon.PLACEHOLDER_ICON_URL;
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

  var webkitTransform = transform ?
      ('scaleX(' + transform.scaleX + ') ' +
      'scaleY(' + transform.scaleY + ') ' +
      'rotate(' + transform.rotate90 * 90 + 'deg)') :
      '';

  for (var i = 0; i < 2; i++) {
    this.wrappers_[i].style.webkitTransform = webkitTransform;

    var img = this.imgs_[i];

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

      function onLoad(image) {
        image.style.maxWidth = '';
        image.style.maxHeight = '';
        resizeToFill(image, image.width / image.height);
      }
      img.onload = onLoad.bind(null, img);
    }

    img.setAttribute('src', url);
  }
};

function ShareMode(container, toolbar, shareActions,
                   onClick, actionCallback, displayStringFunction) {
  ImageEditor.Mode.call(this, 'share');

  this.message_ = null;

  var doc = container.ownerDocument;
  this.button_ = doc.createElement('div');
  this.button_.className = 'button share';
  this.button_.textContent = displayStringFunction('share');
  this.button_.addEventListener('click', onClick);
  toolbar.appendChild(this.button_);

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
