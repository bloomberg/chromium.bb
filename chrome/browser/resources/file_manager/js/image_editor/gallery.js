// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Gallery(container, closeCallback) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.editing_ = false;
  this.closeCallback_ = closeCallback;
  this.onFadeTimeoutBound_ = this.onFadeTimeout_.bind(this);
  this.fadeTimeoutId_ = null;
  this.initDom_();
}

Gallery.open = function(parentDirEntry, entries, closeCallback) {
  var container = document.querySelector('.gallery');
  var gallery = new Gallery(container, closeCallback);
  gallery.load(parentDirEntry, entries);
};

// TODO(dgozman): localization.
Gallery.CLOSE_LABEL = 'Close';
Gallery.EDIT_LABEL = 'Edit';
Gallery.SHARE_LABEL = 'Share';

Gallery.FADE_TIMEOUT = 5000;

Gallery.prototype.initDom_ = function() {
  var doc = this.document_;
  this.container_.addEventListener('keydown', this.onKeyDown_.bind(this));
  this.container_.addEventListener('mousemove', this.onMouseMove_.bind(this));

  this.closeButton_ = doc.createElement('div');
  this.closeButton_.className = 'close';
  this.closeButton_.textContent = Gallery.CLOSE_LABEL;
  this.closeButton_.addEventListener('click', this.closeCallback_);
  this.container_.appendChild(this.closeButton_);

  this.imageContainer_ = doc.createElement('div');
  this.imageContainer_.className = 'image-container';
  this.container_.appendChild(this.imageContainer_);

  this.toolbar_ = doc.createElement('div');
  this.toolbar_.className = 'toolbar';
  this.container_.appendChild(this.toolbar_);

  this.ribbon_ = new Ribbon(this.toolbar_,
      this.onImageSelected_.bind(this));

  this.editBar_  = doc.createElement('div');
  this.editBar_.className = 'edit-bar';
  this.editBar_.setAttribute('hidden', 'hidden');
  this.toolbar_.appendChild(this.editBar_);

  this.editButton_ = doc.createElement('div');
  this.editButton_.className = 'button edit';
  this.editButton_.textContent = Gallery.EDIT_LABEL;
  this.editButton_.addEventListener('click', this.onEdit_.bind(this));
  this.toolbar_.appendChild(this.editButton_);

  this.shareButton_ = doc.createElement('div');
  this.shareButton_.className = 'button share';
  this.shareButton_.textContent = Gallery.SHARE_LABEL;
  this.shareButton_.addEventListener('click', this.onShare_.bind(this));
  this.toolbar_.appendChild(this.shareButton_);

  this.editor_ = new ImageEditor(this.imageContainer_, this.editBar_,
      this.onSave_.bind(this), null /*closeCallback*/);
};

Gallery.prototype.load = function(parentDirEntry, entries) {
  this.editBar_.setAttribute('hidden', 'hidden');
  // Firstchild is the toolbar with buttons, which should be hidden at start.
  this.editBar_.firstChild.setAttribute('hidden', 'hidden');
  this.editButton_.removeAttribute('pressed');
  this.shareButton_.removeAttribute('pressed');
  this.toolbar_.removeAttribute('hidden');
  this.editing_ = false;

  this.ribbon_.setEntries(entries);
  this.parentDirEntry_ = parentDirEntry;

  if (entries.length == 0)
    return;

  this.ribbon_.select(0);
  this.editor_.resizeFrame();

  this.initiateFading_();
};

Gallery.prototype.onSave_ = function(blob) {
  var name = this.entry_.name;
  var ext = '';
  var index = name.lastIndexOf('.');
  if (index != -1) {
    ext = name.substr(index);
    name = name.substr(0, index);
  }
  var now = new Date();
  // TODO(dgozman): better name format.
  name = name + '_Edited_' + now.getFullYear() + '_' +
      (now.getMonth() + 1) + '_' + now.getDate() + '_' +
      now.getHours() + '_' + now.getMinutes() + ext;

  function onError(error) {
    console.log('Error saving from gallery: "' + name + '"');
    console.log(error);
  }

  // TODO(dgozman): Check for existence.
  this.parentDirEntry_.getFile(
      name, {create: true, exclusive: true}, function(fileEntry) {
    fileEntry.createWriter(function(fileWriter) {
      fileWriter.onerror = onError;
      fileWriter.onwritened = function() {
        console.log('Saving from gallery succeeded: "' + name + '"');
      };
      fileWriter.write(blob);
    }, onError);
  }, onError);
};

Gallery.prototype.onImageSelected_ = function(entry) {
  this.entry_ = entry;
  this.editor_.load(entry.toURL());
};

Gallery.prototype.onEdit_ = function(event) {
  var self = this;
  if (this.editing_) {
    this.editBar_.setAttribute('hidden', 'hidden');
    this.editButton_.removeAttribute('pressed');
    this.editing_ = false;
    this.initiateFading_();
    window.setTimeout(function() {
      // Hide the toolbar, so it will not overlap with buttons.
      self.editBar_.firstChild.setAttribute('hidden', 'hidden');
      self.ribbon_.redraw();
    }, 500);
  } else {
    this.cancelFading_();
    // Show the toolbar.
    this.editBar_.firstChild.removeAttribute('hidden');
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
  // TODO(dgozman): implement.
};

Gallery.prototype.onKeyDown_ = function(event) {
  if (this.editing_)
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
  this.toolbar_.setAttribute('hidden', 'hidden');
};

Gallery.prototype.initiateFading_ = function() {
  if (this.editing_ || this.fadeTimeoutId_) {
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
  this.entries_ = [];
  this.images_ = [];
  this.selectedIndex_ = -1;
  this.firstIndex_ = 0;
}

Ribbon.prototype.clear = function() {
  this.bar_.textContent = '';
  this.entries_ = [];
  this.images_ = [];
  this.selectedIndex_ = -1;
  this.firstIndex_ = 0;
};

Ribbon.prototype.add = function(entry) {
  this.entries_.push(entry);
  var index = this.entries_.length - 1;

  var box = this.document_.createElement('div');
  box.className = 'ribbon-image';
  box.addEventListener('click', this.select.bind(this, index));
  this.images_.push(box);

  var img = this.document_.createElement('img');
  img.setAttribute('src', entry.toURL());
  box.appendChild(img);
};

Ribbon.prototype.setEntries = function(entries) {
  this.clear();
  for (var index = 0; index < entries.length; ++index) {
    this.add(entries[index]);
  }

  window.setTimeout(this.redraw.bind(this), 0);
};

Ribbon.prototype.select = function(index) {
  if (index < 0 || index >= this.entries_.length)
    return;
  if (this.selectedIndex_ != -1)
    this.images_[this.selectedIndex_].removeAttribute('selected');
  this.selectedIndex_ = index;
  this.images_[this.selectedIndex_].setAttribute('selected', 'selected');
  if (this.onSelect_)
    this.onSelect_(this.entries_[index]);
};

Ribbon.prototype.redraw = function() {
  this.bar_.textContent = '';

  // TODO(dgozman): get rid of these constants.
  var itemWidth = 49;
  var width = this.bar_.clientWidth - 40;

  var fit = Math.round(Math.floor(width / itemWidth));
  var lastIndex = Math.min(this.entries_.length, this.firstIndex_ + fit);
  this.firstIndex_ = Math.max(0, lastIndex - fit);
  for (var index = this.firstIndex_; index < lastIndex; ++index) {
    this.bar_.appendChild(this.images_[index]);
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
  if (this.firstIndex_ < this.entries_.length - 1) {
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
  if (this.firstIndex_ < this.entries_.length - 1) {
    this.firstIndex_ = this.entries_.length - 1;
    this.redraw();
  }
};
