// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * SelectAlbumDialog contains a message, a list box, an ok button, and a
 * cancel button.
 * Operates on a list of objects representing albums: { name, url, create }.
 * If user chooses to create a new album, result will be a fake album with
 * |create == true|.
 *
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @constructor
 */
function SelectAlbumDialog(parentNode) {
  this.parentNode_ = parentNode;
  this.document_ = parentNode.ownerDocument;

  this.container_ = this.document_.createElement('div');
  this.container_.className = 'select-album-dialog-container';
  this.container_.addEventListener('keydown',
      this.onContainerKeyDown_.bind(this));

  this.shield_ = this.document_.createElement('div');
  this.shield_.className = 'select-album-dialog-shield';
  this.container_.appendChild(this.shield_);

  this.frame_ = this.document_.createElement('div');
  this.frame_.className = 'select-album-dialog-frame';
  this.container_.appendChild(this.frame_);

  this.caption_ = this.document_.createElement('div');
  this.caption_.className = 'select-album-dialog-caption';
  this.frame_.appendChild(this.caption_);

  this.list_ = new cr.ui.List();
  this.list_.classList.add('select-album-list');
  this.frame_.appendChild(this.list_);

  this.dataModel_ = this.list_.dataModel = new cr.ui.ArrayDataModel([]);
  this.selectionModel_ = this.list_.selectionModel =
      new cr.ui.ListSingleSelectionModel();
  this.selectionModel_.addEventListener('change',
      this.onSelectionChanged_.bind(this));

  // TODO(dgozman): add shades at top and bottom of the list.
  // List has max-height defined at css, so that list grows automatically,
  // but doesn't exceed predefined size.
  this.list_.autoExpands = true;
  this.list_.activateItemAtIndex = this.activateItemAtIndex_.bind(this);
  // Binding stuff doesn't work with constructors, so we have to create
  // closure here.
  var self = this;
  this.list_.itemConstructor = function(item) {
    return self.renderItem(item);
  };

  var buttons = this.document_.createElement('div');
  buttons.className = 'select-album-dialog-buttons';
  this.frame_.appendChild(buttons);

  this.okButton_ = this.document_.createElement('button');
  this.okButton_.className = 'no-icon';
  this.okButton_.addEventListener('click', this.onOkClick_.bind(this));
  buttons.appendChild(this.okButton_);

  this.cancelButton_ = this.document_.createElement('button');
  this.cancelButton_.className = 'no-icon';
  this.cancelButton_.textContent =
      loadTimeData.getString('PHOTO_IMPORT_CANCEL_BUTTON');
  this.cancelButton_.addEventListener('click', this.onCancelClick_.bind(this));
  buttons.appendChild(this.cancelButton_);

  this.nameEdit_ = this.document_.createElement('input');
  this.nameEdit_.setAttribute('type', 'text');
  this.nameEdit_.className = 'name';
  this.nameEdit_.addEventListener('input',
      this.updateOkButtonEnabled_.bind(this));
}

SelectAlbumDialog.prototype = {
  __proto__: cr.ui.dialogs.BaseDialog.prototype
};

/**
 * Renders item for list.
 * @param {Object} item Item to render.
 * @return {HTMLLIElement} Rendered item.
 */
SelectAlbumDialog.prototype.renderItem = function(item) {
  var result = this.document_.createElement('li');

  var frame = this.document_.createElement('div');
  frame.className = 'img-frame';
  result.appendChild(frame);

  var box = this.document_.createElement('div');
  box.className = 'img-container';
  frame.appendChild(box);

  if (item.create) {
    result.appendChild(this.nameEdit_);
    this.nameEdit_.value = item.name;
  } else {
    var name = this.document_.createElement('div');
    name.className = 'name';
    name.textContent = item.name;
    result.appendChild(name);
  }

  cr.defineProperty(result, 'lead', cr.PropertyKind.BOOL_ATTR);
  cr.defineProperty(result, 'selected', cr.PropertyKind.BOOL_ATTR);

  new ThumbnailLoader(item.url).load(box, ThumbnailLoader.FillMode.FILL);

  return result;
};

/**
 * Shows dialog.
 *
 * @param {string} message Message in dialog caption.
 * @param {Array} items Albums to render in list.
 * @param {string} defaultNewName Default name of the new album.
 * @param {string} okCaption Text on the ok button.
 * @param {function} onOk Callback function.
 */
SelectAlbumDialog.prototype.show = function(
    message, items, defaultNewName, okCaption, onOk) {

  this.onOk_ = onOk;
  this.okButton_.textContent = okCaption;
  this.caption_.textContent = message;

   // Fake item to create new album.
  var newAlbum = {
    create: true,
    name: defaultNewName,
    url: chrome.extension.getURL('../../images/photo/new_album.png')
  };

  this.list_.startBatchUpdates();
  this.dataModel_.splice(0, this.dataModel_.length);
  this.dataModel_.push(newAlbum);
  for (var i = 0; i < items.length; i++) {
    this.dataModel_.push(items[i]);
  }
  this.selectionModel_.selectedIndex = 0;
  this.list_.endBatchUpdates();

  this.parentNode_.appendChild(this.container_);
};

/**
 * Hides dialog.
 */
SelectAlbumDialog.prototype.hide = function() {
  this.parentNode_.removeChild(this.container_);
};

/**
 * List activation handler. Closes dialog and calls 'ok' callback.
 *
 * @param {number} index Activated index.
 * @private
 */
SelectAlbumDialog.prototype.activateItemAtIndex_ = function(index) {
  if (this.okButton_.disabled) return;
  this.hide();
  var album = this.dataModel_.item(index);
  if (index == 0)
    album.name = this.nameEdit_.value;
  this.onOk_(album);
};

/**
 * Closes dialog and invokes callback with currently-selected item.
 * @private
 */
SelectAlbumDialog.prototype.onOkClick_ = function() {
  this.activateItemAtIndex_(this.selectionModel_.selectedIndex);
};

/**
 * Closes dialog.
 * @private
 */
SelectAlbumDialog.prototype.onCancelClick_ = function() {
  this.hide();
};

/**
 * Event handler for keydown event.
 * @param {Event} event The event.
 * @private
 */
SelectAlbumDialog.prototype.onContainerKeyDown_ = function(event) {
  // Handle Escape.
  if (event.keyCode == 27) {
    this.onCancelClick_(event);
    event.preventDefault();
  } else if (event.keyCode == 13) {
    this.onOkClick_();
    event.preventDefault();
  }
};

/**
 * Event handler for selection change.
 * @param {Event} event The event.
 * @private
 */
SelectAlbumDialog.prototype.onSelectionChanged_ = function(event) {
  if (this.selectionModel_.selectedIndex == 0) {
    setTimeout(this.nameEdit_.focus.bind(this.nameEdit_), 0);
  } else {
    this.nameEdit_.blur();
    this.list_.focus();
  }
  this.updateOkButtonEnabled_();
};

/**
 * Updates ok button.
 * @private
 */
SelectAlbumDialog.prototype.updateOkButtonEnabled_ = function() {
  this.okButton_.disabled = this.selectionModel_.selectedIndex == 0 &&
      this.nameEdit_.value == '';
};
