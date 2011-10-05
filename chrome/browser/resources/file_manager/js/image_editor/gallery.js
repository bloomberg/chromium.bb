// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Image gallery for viewing and editing image files.
 *
 * @param {HTMLDivElement} container
 * @param {function} closeCallback
 * @param {MetadataProvider} metadataProvider
 * @param {Array.<Object>} shareActions
 */
function Gallery(container, closeCallback, metadataProvider, shareActions) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.editing_ = false;
  this.sharing_ = false;
  this.closeCallback_ = closeCallback;
  this.metadataProvider_ = metadataProvider;
  this.onFadeTimeoutBound_ = this.onFadeTimeout_.bind(this);
  this.fadeTimeoutId_ = null;

  this.initDom_(shareActions);
}

Gallery.open = function(parentDirEntry, items, selectedItem,
                        closeCallback, metadataProvider, shareActions) {
  var container = document.querySelector('.gallery');
  container.innerHTML = '';
  var gallery = new Gallery(
      container, closeCallback, metadataProvider, shareActions);
  gallery.load(parentDirEntry, items, selectedItem);
};

// TODO(kaznacheev): localization.
Gallery.displayStrings = {
  edit: 'Edit',
  share: 'Share',
  autofix: 'Auto-fix',
  crop: 'Crop',
  exposure: 'Brightness',
  brightness: 'Brightness',
  contrast: 'Contrast',
  'rotate-left': 'Left',
  'rotate-right': 'Right',
  'enter-when-done': 'Press Enter when done',
  fixed: 'Fixed',
  undo: 'Undo',
  redo: 'Redo'
};


Gallery.editorModes = [
  new ImageEditor.Mode.InstantAutofix(),
  new ImageEditor.Mode.Crop(),
  new ImageEditor.Mode.Exposure(),
  new ImageEditor.Mode.OneClick('rotate-left', new Command.Rotate(-1)),
  new ImageEditor.Mode.OneClick('rotate-right', new Command.Rotate(1))
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
    doc.body.removeEventListener('keydown', window.galleryMouseMove);
  }

  window.galleryKeyDown = this.onKeyDown_.bind(this);
  doc.body.addEventListener('keydown', window.galleryKeyDown);

  window.galleryMouseMove = this.onMouseMove_.bind(this);
  doc.body.addEventListener('mousemove', window.galleryMouseMove);

  this.closeButton_ = doc.createElement('div');
  this.closeButton_.className = 'close';
  this.closeButton_.addEventListener('click', this.onClose_.bind(this));
  this.container_.appendChild(this.closeButton_);

  this.imageContainer_ = doc.createElement('div');
  this.imageContainer_.className = 'image-container';
  this.container_.appendChild(this.imageContainer_);

  this.toolbar_ = doc.createElement('div');
  this.toolbar_.className = 'toolbar';
  this.container_.appendChild(this.toolbar_);

  this.ribbonSpacer_ = doc.createElement('div');
  this.ribbonSpacer_.className = 'ribbon-spacer';
  this.toolbar_.appendChild(this.ribbonSpacer_);

  this.toolbar_.addEventListener('mouseover',
      this.enableFading_.bind(this, false));
  this.toolbar_.addEventListener('mouseout',
      this.enableFading_.bind(this, true));

  this.arrowLeft_ = this.document_.createElement('div');
  this.arrowLeft_.className = 'arrow left';
  this.container_.appendChild(this.arrowLeft_);

  this.arrowRight_ = this.document_.createElement('div');
  this.arrowRight_.className = 'arrow right';
  this.container_.appendChild(this.arrowRight_);

  this.ribbon_ = new Ribbon(this.ribbonSpacer_, this.onSelect_.bind(this),
      this.arrowLeft_, this.arrowRight_);

  this.editBar_  = doc.createElement('div');
  this.editBar_.className = 'edit-bar';
  this.toolbar_.appendChild(this.editBar_);

  this.editButton_ = doc.createElement('div');
  this.editButton_.className = 'button edit';
  this.editButton_.textContent = Gallery.displayStrings['edit'];
  this.editButton_.addEventListener('click', this.onEdit_.bind(this));
  this.toolbar_.appendChild(this.editButton_);

  this.shareButton_ = doc.createElement('div');
  this.shareButton_.className = 'button share';
  this.shareButton_.textContent = Gallery.displayStrings['share'];
  this.shareButton_.addEventListener('click', this.onShare_.bind(this));
  if (shareActions.length > 0) {
    this.toolbar_.appendChild(this.shareButton_);
  }

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

  this.shareMenu_ = doc.createElement('div');
  this.shareMenu_.className = 'share-menu';
  this.shareMenu_.setAttribute('hidden', 'hidden');
  for (var index = 0; index < shareActions.length; index++) {
    var action = shareActions[index];
    var row = doc.createElement('div');
    var img = doc.createElement('img');
    img.src = action.iconUrl;
    row.appendChild(img);
    row.appendChild(doc.createTextNode(action.title));
    row.addEventListener('click', this.onActionExecute_.bind(this, action));
    this.shareMenu_.appendChild(row);
  }
  if (shareActions.length > 0) {
    this.container_.appendChild(this.shareMenu_);
  }

  this.editor_ = new ImageEditor(
      this.imageContainer_,
      this.editBarMain_,
      this.editBarModeWrapper_,
      Gallery.editorModes,
      Gallery.displayStrings);

  this.imageView_ = this.editor_.getImageView();

  this.editor_.trackWindow(doc.defaultView);
};

Gallery.prototype.load = function(parentDirEntry, items, selectedItem) {
  this.parentDirEntry_ = parentDirEntry;

  this.editButton_.removeAttribute('pressed');
  this.shareButton_.removeAttribute('pressed');

  this.shareMenu_.setAttribute('hidden', 'hidden');
  this.editing_ = false;
  this.sharing_ = false;

  this.cancelFading_();
  this.initiateFading_();

  var urls = [];
  var selectedURL;

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
      urls.push(item);
      if (selected) selectedURL = item;
    } else {
      console.error('Unsupported image type');
    }
  }

  if (urls.length == 0)
    throw new Error('Cannot open the gallery for 0 items');

  selectedURL = selectedURL || urls[0];

  var self = this;

  function initRibbon() {
    self.currentItem_ =
        self.ribbon_.load(urls, selectedURL, self.metadataProvider_);
    // Flash the ribbon briefly to let the user know it is there.
    self.initiateFading_(Gallery.FIRST_FADE_TIMEOUT);
  }

  // Initialize the ribbon only after the selected image is fully loaded.
  this.metadataProvider_.fetch(selectedURL, function (metadata) {
    self.editor_.openSession(selectedURL, metadata, 0, initRibbon);
  });
};

Gallery.prototype.saveChanges_ = function(opt_callback) {
  var item = this.currentItem_;
  var self = this;
  this.editor_.closeSession(function(canvas, modified) {
    if (modified) {
      item.save(
          self.parentDirEntry_, self.metadataProvider_, canvas, opt_callback);
    } else {
      if (opt_callback) opt_callback();
    }
  });
};

Gallery.prototype.onActionExecute_ = function(action) {
  var item = this.currentItem_;
  if (item) {
    this.onShare_();
    this.saveChanges_(function() {
      action.execute([item.getUrl()]);
    });
  }
};

Gallery.prototype.onClose_ = function() {
  // TODO: handle write errors gracefully (suggest retry or saving elsewhere).
  this.saveChanges_(this.closeCallback_);
};

Gallery.prototype.onSelect_ = function(item) {
  if (this.currentItem_ == item)
    return;

  this.saveChanges_();
  var slide = item.getIndex() - this.currentItem_.getIndex();

  this.currentItem_ = item;

  this.editor_.openSession(
      this.currentItem_.getContent(), this.currentItem_.getMetadata(), slide);
};

Gallery.prototype.onEdit_ = function() {
  if (this.editing_) {
    this.editor_.leaveModeGently();
    this.initiateFading_();
  } else {
    this.cancelFading_();
  }
  this.editing_ = !this.editing_;
  ImageUtil.setAttribute(this.container_, 'editing', this.editing_);
  ImageUtil.setAttribute(this.editButton_, 'pressed', this.editing_);
};

Gallery.prototype.onShare_ = function(event) {
  ImageUtil.setAttribute(this.container_, 'tools', true);

  if (this.sharing_) {
    this.shareMenu_.setAttribute('hidden', 'hidden');
  } else {
    this.shareMenu_.removeAttribute('hidden');
  }
  this.sharing_ = !this.sharing_;
};

Gallery.prototype.onKeyDown_ = function(event) {
  if (this.sharing_)
    return;

  if (this.editing_ && this.editor_.onKeyDown(event))
    return;

  switch (event.keyIdentifier) {
    case 'U+001B':  // Escape
      if (this.editing_) {
        this.onEdit_();
      } else {
        this.onClose_();
      }
      break;

    case 'U+0045':  // 'e'
      if (!this.editing_)
        this.onEdit_();
      break;

    case 'Home':
      this.ribbon_.selectFirst();
      break;
    case 'Left':
      this.ribbon_.selectPrevious();
      break;
    case 'Right':
      this.ribbon_.selectNext();
      break;
    case 'End':
      this.ribbon_.selectLast();
      break;
  }
};

Gallery.prototype.onMouseMove_ = function(e) {
  this.cancelFading_();
  this.initiateFading_();
};

Gallery.prototype.onFadeTimeout_ = function() {
  this.fadeTimeoutId_ = null;
  if (this.editing_ || this.sharing_) return;
  this.container_.removeAttribute('tools');
};

Gallery.prototype.enableFading_ = function(on) {
  this.fadingEnabled_ = on;
  if (this.fadingEnabled_)
    this.initiateFading_();
  else
    this.cancelFading_();
};

Gallery.prototype.initiateFading_ = function(opt_timeout) {
  if (!this.fadingEnabled_)
      return;

  if (this.editing_ || this.sharing_ || this.fadeTimeoutId_) {
    return;
  }
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

function Ribbon(container, onSelect, arrowLeft, arrowRight) {
  this.container_ = container;
  this.document_ = container.ownerDocument;

  this.arrowLeft_ = arrowLeft;
  this.arrowLeft_.addEventListener('click', this.selectPrevious.bind(this));

  this.arrowRight_ = arrowRight;
  this.arrowRight_.addEventListener('click', this.selectNext.bind(this));

  this.fadeLeft_ = this.document_.createElement('div');
  this.fadeLeft_.className = 'fade left';
  this.container_.appendChild(this.fadeLeft_);

  this.bar_ = this.document_.createElement('div');
  this.bar_.className = 'ribbon';
  this.container_.appendChild(this.bar_);

  this.fadeRight_ = this.document_.createElement('div');
  this.fadeRight_.className = 'fade right';
  this.container_.appendChild(this.fadeRight_);

  this.onSelect_ = onSelect;
  this.items_ = [];
  this.selectedIndex_ = -1;
  this.firstVisibleIndex_ = 0;
  this.lastVisibleIndex_ = 0;
}

Ribbon.prototype.clear = function() {
  this.bar_.textContent = '';
  this.items_ = [];
  this.selectedIndex_ = -1;
  this.firstVisibleIndex_ = 0;
  this.lastVisibleIndex_ = 0;
};

Ribbon.prototype.add = function(url, metadataProvider) {
  var index = this.items_.length;
  var selectClosure = this.select.bind(this, index);
  var item = new Ribbon.Item(index, url, this.document_, selectClosure);
  this.items_.push(item);
  var self = this;
  metadataProvider.fetch(url, function(metadata) {
    item.setMetadata(metadata);
    if (item.isSelected()) {
      self.onSelect_(item);
    }
  });
  return item;
};

Ribbon.prototype.load = function(urls, selectedURL, metadataProvider) {
  this.clear();
  var selectedIndex = -1;
  for (var index = 0; index < urls.length; ++index) {
    var item = this.add(urls[index], metadataProvider);
    if (urls[index] == selectedURL)
      selectedIndex = index;
  }
  this.select(selectedIndex);
  window.setTimeout(this.redraw.bind(this), 0);
  return this.items_[this.selectedIndex_];
};

Ribbon.prototype.select = function(index) {
  if (index == this.selectedIndex_)
    return;  // Do not reselect.

  if (this.selectedIndex_ != -1) {
    this.items_[this.selectedIndex_].select(false);
  }

  this.selectedIndex_ = index;

  if (this.selectedIndex_ < this.firstVisibleIndex_) {
    if (this.selectedIndex_ == this.firstVisibleIndex_ - 1) {
      this.scrollLeft();
    } else {
      this.redraw();
    }
  }
  if (this.selectedIndex_ > this.lastVisibleIndex_) {
    if (this.selectedIndex_ == this.lastVisibleIndex_ + 1) {
      this.scrollRight();
    } else {
      this.redraw();
    }
  }

  if (this.selectedIndex_ != -1) {
    var selectedItem = this.items_[this.selectedIndex_];
    selectedItem.select(true);
    if (selectedItem.getMetadata()) {
      this.onSelect_(selectedItem);
    } // otherwise onSelect is called from the metadata callback.
  }

  ImageUtil.setAttribute(this.arrowLeft_, 'active', this.selectedIndex_ > 0);
  ImageUtil.setAttribute(this.arrowRight_, 'active',
      this.selectedIndex_ + 1 < this.items_.length);

  ImageUtil.setAttribute(this.fadeLeft_, 'active', this.firstVisibleIndex_ > 0);
  ImageUtil.setAttribute(this.fadeRight_, 'active',
      this.lastVisibleIndex_ + 1 < this.items_.length);
};

Ribbon.prototype.redraw = function() {
  this.bar_.textContent = '';

  // The thumbnails are square.
  var itemWidth = this.bar_.parentNode.clientHeight;
  var width = this.bar_.parentNode.clientWidth;

  var fullItems = Math.floor(width / itemWidth);

  this.bar_.style.width = fullItems * itemWidth + 'px';

  this.firstVisibleIndex_ =
      Math.min(this.firstVisibleIndex_, this.selectedIndex_);

  this.lastVisibleIndex_ =
      Math.min(this.items_.length, this.firstVisibleIndex_ + fullItems) - 1;

  this.lastVisibleIndex_ =
      Math.max(this.lastVisibleIndex_, this.selectedIndex_);

  this.firstVisibleIndex_ =
      Math.max(0, this.lastVisibleIndex_ - fullItems + 1);

  fullItems = this.lastVisibleIndex_ - this.firstVisibleIndex_ + 1;

  for (var index = this.firstVisibleIndex_;
       index <= this.lastVisibleIndex_;
       ++index) {
    this.bar_.appendChild(this.items_[index].getBox());
  }
};

// TODO(kaznacheev) The animation logic below does not really work for fast
// repetitive scrolling.

Ribbon.prototype.scrollLeft = function() {
  console.log('scrollLeft');
  this.firstVisibleIndex_--;
  var fadeIn = this.items_[this.firstVisibleIndex_].getBox();
  ImageUtil.setAttribute(fadeIn, 'shifted', true);
  this.bar_.insertBefore(fadeIn, this.bar_.firstElementChild);
  setTimeout(
      function() { ImageUtil.setAttribute(fadeIn, 'shifted', false) },
      0);

  var pushOut = this.bar_.lastElementChild;
  pushOut.parentNode.removeChild(pushOut);

  this.lastVisibleIndex_--;
};

Ribbon.prototype.scrollRight = function() {
  console.log('scrollRight');

  var fadeOut = this.items_[this.firstVisibleIndex_].getBox();
  ImageUtil.setAttribute(fadeOut, 'shifted', true);
  setTimeout(function() {
    fadeOut.parentNode.removeChild(fadeOut);
    ImageUtil.setAttribute(fadeOut, 'shifted', false);
  }, 500);

  this.firstVisibleIndex_++;
  this.lastVisibleIndex_++;

  var pushIn = this.items_[this.lastVisibleIndex_].getBox();
  this.bar_.appendChild(pushIn);
};

Ribbon.prototype.selectPrevious = function() {
  if (this.selectedIndex_ > 0)
    this.select(this.selectedIndex_ - 1);
};

Ribbon.prototype.selectNext = function() {
  if (this.selectedIndex_ < this.items_.length - 1)
    this.select(this.selectedIndex_ + 1);
};

Ribbon.prototype.selectFirst = function() {
  this.select(0);
};

Ribbon.prototype.selectLast = function() {
  this.select(this.items_.length - 1);
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
};

Ribbon.Item.prototype.getIndex = function () { return this.index_ };

Ribbon.Item.prototype.getBox = function () { return this.box_ };

Ribbon.Item.prototype.isOriginal = function () { return this.original_ };

Ribbon.Item.prototype.getUrl = function () { return this.url_ };

Ribbon.Item.prototype.isSelected = function() {
  return this.box_.hasAttribute('selected');
};

Ribbon.Item.prototype.select = function(on) {
  ImageUtil.setAttribute(this.box_, 'selected', on);
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
    var signaturePos = name.indexOf(Ribbon.Item.COPY_SIGNATURE);
    if (signaturePos >= 0) {
      // The file is likely to be a copy created during a previous session.
      // Replace the signature instead of appending a new one.
      name = name.substr(0, signaturePos);
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

Ribbon.Item.prototype.setMetadata = function(metadata) {
  this.metadata_ = metadata;

  var transform =
      metadata.thumbnailURL ?
      metadata.thumbnailTransform :
      metadata.imageTransform;

  this.wrapper_.style.webkitTransform = transform ?
      ('scaleX(' + transform.scaleX + ') ' +
      'scaleY(' + transform.scaleY + ') ' +
      'rotate(' + transform.rotate90 * 90 + 'deg)') :
      '';

  function percent(ratio) { return Math.round(ratio * 100) + '%' }

  function resizeToFill(img, aspect) {
    if ((aspect > 1)) {
      img.style.height = percent(1);
      img.style.width = percent(aspect);
      img.style.marginLeft = percent((1 - aspect) / 2);
    } else {
      aspect = 1 / aspect;
      img.style.width = percent(1);
      img.style.height = percent(aspect);
      img.style.marginTop = percent((1 - aspect) / 2);
    }
  }

  if (metadata.width && metadata.height) {
    var aspect = metadata.width / metadata.height;
    if (transform && transform.rotate90) {
      aspect = 1 / aspect;
    }
    resizeToFill(this.img_, aspect);
  } else {
    // No metadata available, loading the thumbnail first, then adjust the size.
    this.img_.maxWidth = '100%';
    this.img_.maxHeight = '100%';

    var img = this.img_;
    this.img_.onload = function() {
      img.maxWidth = 'none';
      img.maxHeight = 'none';
      resizeToFill(img, img.width / img.height);
    }
  }

  this.img_.setAttribute('src', metadata.thumbnailURL || this.url_);
};
