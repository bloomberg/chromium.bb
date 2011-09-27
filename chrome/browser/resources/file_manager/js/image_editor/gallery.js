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

Gallery.open = function(
    parentDirEntry, urls, closeCallback, metadataProvider, shareActions) {
  var container = document.querySelector('.gallery');
  container.innerHTML = '';
  var gallery = new Gallery(container, closeCallback, metadataProvider,
      shareActions);
  gallery.load(parentDirEntry, urls);
};

// TODO(kaznacheev): localization.
Gallery.displayStrings = {
  close: 'Close',
  edit: 'Edit',
  share: 'Share',
  autofix: 'Auto-fix',
  crop: 'Crop',
  exposure: 'Brightness / contrast',
  brightness: 'Brightness',
  contrast: 'Contrast',
  rotate: 'Rotate',
  undo: 'Undo'
};


Gallery.editorModes = [
  ImageEditor.Mode.InstantAutofix,
  ImageEditor.Mode.Crop,
  ImageEditor.Mode.Exposure,
  ImageEditor.Mode.InstantRotate
];

Gallery.FADE_TIMEOUT = 5000;

Gallery.prototype.initDom_ = function(shareActions) {
  var doc = this.document_;
  this.container_.addEventListener('keydown', this.onKeyDown_.bind(this));
  this.container_.addEventListener('mousemove', this.onMouseMove_.bind(this));

  this.closeButton_ = doc.createElement('div');
  this.closeButton_.className = 'close';
  this.closeButton_.textContent = Gallery.displayStrings['close'];
  this.closeButton_.addEventListener('click', this.onClose_.bind(this));
  this.container_.appendChild(this.closeButton_);

  this.imageContainer_ = doc.createElement('div');
  this.imageContainer_.className = 'image-container';
  this.container_.appendChild(this.imageContainer_);

  this.toolbar_ = doc.createElement('div');
  this.toolbar_.className = 'toolbar';
  this.container_.appendChild(this.toolbar_);

  this.ribbon_ = new Ribbon(this.toolbar_, this.onSelect_.bind(this));

  this.editBar_  = doc.createElement('div');
  this.editBar_.className = 'edit-bar';
  this.editBar_.setAttribute('hidden', 'hidden');
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
  this.editBarMain_.setAttribute('hidden', 'hidden');
  this.editBar_.appendChild(this.editBarMain_);

  this.editBarMode_  = doc.createElement('div');
  this.editBarMode_.className = 'edit-modal';
  this.editBarMode_.setAttribute('hidden', 'hidden');
  this.editBar_.appendChild(this.editBarMode_);

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
      this.editBarMode_,
      Gallery.editorModes,
      Gallery.displayStrings);
};

Gallery.prototype.load = function(parentDirEntry, urls) {
  this.editBar_.setAttribute('hidden', 'hidden');
  this.editBarMain_.setAttribute('hidden', 'hidden');
  this.editButton_.removeAttribute('pressed');
  this.shareButton_.removeAttribute('pressed');
  this.toolbar_.removeAttribute('hidden');
  this.shareMenu_.setAttribute('hidden', 'hidden');
  this.editing_ = false;
  this.sharing_ = false;

  if (urls.length == 0)
    return;

  // TODO(kaznacheev): instead of always selecting the 0-th url
  // select the url passed from the FileManager.
  this.ribbon_.load(urls, urls[0], this.metadataProvider_);
  this.parentDirEntry_ = parentDirEntry;

  this.initiateFading_();
};

Gallery.prototype.saveChanges_ = function(opt_callback) {
  if (!this.editor_.isModified()) {
    if (opt_callback) opt_callback();
    return;
  }

  var currentItem = this.currentItem_;

  var metadataEncoder = this.editor_.encodeMetadata();
  var canvas = this.editor_.getBuffer().getContent().detachCanvas();

  currentItem.overrideContent(canvas, metadataEncoder.getMetadata());

  if (currentItem.isFromLocalFile()) {
    var newFile = currentItem.isOriginal();
    var name = currentItem.getCopyName();

    var self = this;

    function onSuccess(url) {
      console.log('Saved from gallery', name);
      // Force the metadata provider to reread the metadata from the file.
      self.metadataProvider_.reset(url);
      currentItem.onSaveSuccess(url);
      if (opt_callback) opt_callback();
    }

    function onError(error) {
      console.log('Error saving from gallery', name, error);
      currentItem.onSaveError(error);
      if (opt_callback) opt_callback();
    }

    this.parentDirEntry_.getFile(
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
  } else {
    // This branch is needed only for gallery_demo.js
    currentItem.onSaveSuccess(
        canvas.toDataURL(metadataEncoder.getMetadata().mimeType));
    if (opt_callback) opt_callback();
  }
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
  if (this.currentItem_) {
    this.saveChanges_();
  }

  this.currentItem_ = item;

  this.editor_.load(this.currentItem_.getContent(),
                    ImageUtil.deepCopy(this.currentItem_.getMetadata()));
};

Gallery.prototype.onEdit_ = function(event) {
  this.toolbar_.removeAttribute('hidden');

  var self = this;
  if (this.editing_) {
    this.editor_.onModeLeave();
    this.editBar_.setAttribute('hidden', 'hidden');
    this.editButton_.removeAttribute('pressed');
    this.editing_ = false;
    this.initiateFading_();
    window.setTimeout(function() {
      // Hide the toolbar, so it will not overlap with buttons.
      self.editBarMain_.setAttribute('hidden', 'hidden');
      self.ribbon_.redraw();
    }, 500);
  } else {
    this.cancelFading_();
    // Show the toolbar.
    this.editBarMain_.removeAttribute('hidden');
    // Use setTimeout, so computed style will be recomputed.
    window.setTimeout(function() {
      self.editBar_.removeAttribute('hidden');
      self.editButton_.setAttribute('pressed', 'pressed');
      self.editing_ = true;
      self.ribbon_.redraw();
    }, 0);
  }
};

Gallery.prototype.onShare_ = function(event) {
  this.toolbar_.removeAttribute('hidden');

  if (this.sharing_) {
    this.shareMenu_.setAttribute('hidden', 'hidden');
  } else {
    this.shareMenu_.removeAttribute('hidden');
  }
  this.sharing_ = !this.sharing_;
};

Gallery.prototype.onKeyDown_ = function(event) {
  if (this.editing_ || this.sharing_)
    return;
  switch (event.keyIdentifier) {
    case 'Home':
      this.ribbon_.scrollToFirst();
      break;
    case 'Left':
      this.ribbon_.scrollLeft();
      break;
    case 'Right':
      this.ribbon_.scrollRight();
      break;
    case 'End':
      this.ribbon_.scrollToLast();
      break;
  }
};

Gallery.prototype.onMouseMove_ = function(e) {
  this.cancelFading_();
  this.toolbar_.removeAttribute('hidden');
  this.initiateFading_();
};

Gallery.prototype.onFadeTimeout_ = function() {
  this.fadeTimeoutId_ = null;
  if (this.editing_ || this.sharing_) return;
  this.toolbar_.setAttribute('hidden', 'hidden');
};

Gallery.prototype.initiateFading_ = function() {
  if (this.editing_ || this.sharing_ || this.fadeTimeoutId_) {
    return;
  }
  this.fadeTimeoutId_ = window.setTimeout(
      this.onFadeTimeoutBound_, Gallery.FADE_TIMEOUT);
};

Gallery.prototype.cancelFading_ = function() {
  if (this.fadeTimeoutId_) {
    window.clearTimeout(this.fadeTimeoutId_);
    this.fadeTimeoutId_ = null;
  }
};

function Ribbon(parentNode, onSelect) {
  this.container_ = parentNode;
  this.document_ = parentNode.ownerDocument;

  this.left_ = this.document_.createElement('div');
  this.left_.className = 'ribbon-left';
  this.left_.addEventListener('click', this.scrollLeft.bind(this));
  this.container_.appendChild(this.left_);

  this.bar_ = this.document_.createElement('div');
  this.bar_.className = 'ribbon';
  this.container_.appendChild(this.bar_);

  this.right_ = this.document_.createElement('div');
  this.right_.className = 'ribbon-right';
  this.right_.addEventListener('click', this.scrollRight.bind(this));
  this.container_.appendChild(this.right_);

  this.spacer_ = this.document_.createElement('div');
  this.spacer_.className = 'ribbon-spacer';

  this.onSelect_ = onSelect;
  this.items_ = [];
  this.selectedItem_ = null;
  this.firstIndex_ = 0;
}

Ribbon.prototype.clear = function() {
  this.bar_.textContent = '';
  this.items_ = [];
  this.selectedItem_ = null;
  this.firstIndex_ = 0;
};

Ribbon.prototype.add = function(url, selected, metadataProvider) {
  var index = this.items_.length;
  var selectClosure = this.select.bind(this);
  var item =
      new Ribbon.Item(this.document_, url, selectClosure);
  this.items_.push(item);
  metadataProvider.fetch(url, function(metadata) {
    item.setMetadata(metadata);
    if (selected) selectClosure(item);
  });
};

Ribbon.prototype.load = function(urls, selectedUrl, metadataProvider) {
  this.clear();
  for (var index = 0; index < urls.length; ++index) {
    this.add(urls[index], urls[index] == selectedUrl, metadataProvider);
  }

  window.setTimeout(this.redraw.bind(this), 0);
};

Ribbon.prototype.select = function(item) {
  if (this.selectedItem_) {
    this.selectedItem_.select(false);
  }

  this.selectedItem_ = item;

  if (this.selectedItem_) {
    this.selectedItem_.select(true);
    if (this.onSelect_)
      this.onSelect_(this.selectedItem_);
  }
};

Ribbon.prototype.redraw = function() {
  this.bar_.textContent = '';

  // TODO(dgozman): get rid of these constants.
  var itemWidth = 49;
  var width = this.bar_.clientWidth - 40;

  var fit = Math.round(Math.floor(width / itemWidth));
  var lastIndex = Math.min(this.items_.length, this.firstIndex_ + fit);
  this.firstIndex_ = Math.max(0, lastIndex - fit);
  for (var index = this.firstIndex_; index < lastIndex; ++index) {
    this.bar_.appendChild(this.items_[index].getBox());
  }
  this.bar_.appendChild(this.spacer_);
};

Ribbon.prototype.scrollLeft = function() {
  if (this.firstIndex_ > 0) {
    this.firstIndex_--;
    this.redraw();
  }
};

Ribbon.prototype.scrollRight = function() {
  if (this.firstIndex_ < this.items_.length - 1) {
    this.firstIndex_++;
    this.redraw();
  }
};

Ribbon.prototype.scrollToFirst = function() {
  if (this.firstIndex_ > 0) {
    this.firstIndex_ = 0;
    this.redraw();
  }
};

Ribbon.prototype.scrollToLast = function() {
  if (this.firstIndex_ < this.items_.length - 1) {
    this.firstIndex_ = this.items_.length - 1;
    this.redraw();
  }
};


Ribbon.Item = function(document, url, selectClosure) {
  this.url_ = url;

  this.img_ = document.createElement('img');

  this.box_ = document.createElement('div');
  this.box_.className = 'ribbon-image';
  this.box_.addEventListener('click', selectClosure.bind(null, this));
  this.box_.appendChild(this.img_);

  this.original_ = true;
};

Ribbon.Item.prototype.getBox = function () { return this.box_ };

Ribbon.Item.prototype.isOriginal = function () { return this.original_ };

Ribbon.Item.prototype.getUrl = function () { return this.url_ };

Ribbon.Item.prototype.select = function(on) {
  if (on)
    this.box_.setAttribute('selected', 'selected');
  else
    this.box_.removeAttribute('selected');
};

// TODO: Localize?
Ribbon.Item.COPY_SIGNATURE = '_Edited_';

Ribbon.Item.prototype.isFromLocalFile = function () {
  return this.url_.indexOf('filesystem:') == 0;
};

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

  this.box_.style.webkitTransform = transform ?
      ('scaleX(' + transform.scaleX + ') ' +
      'scaleY(' + transform.scaleY + ') ' +
      'rotate(' + transform.rotate90 * 90 + 'deg)') :
      '';

  this.img_.setAttribute('src', metadata.thumbnailURL || this.url_);
};
