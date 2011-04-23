// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Item used in preview grid view.
 * @param {picasa.LocalFile} dataItem Data item.
 * @extends {cr.ui.GridItem}
 */
function PreviewItem(dataItem) {
  var item = new cr.ui.GridItem(dataItem);
  item.__proto__ = PreviewItem.prototype;
  return item;
}

PreviewItem.prototype = {
  __proto__: cr.ui.GridItem.prototype,

  /**
   * Called to initialize element.
   */
  decorate: function() {
    cr.ui.GridItem.prototype.decorate.call(this, arguments);
    this.textContent = '';
    this.className = 'preview-item';

    var div = this.ownerDocument.createElement('div');
    this.appendChild(div);

    var img = this.ownerDocument.createElement('img');
    this.dataItem.showInImage(img);
    div.appendChild(img);
  }
};


/**
 * UploadPage constructor.
 *
 * UploadPage object is responsible for manipulating upload page.
 */
function UploadPage() {
  this.files_ = new cr.ui.ArrayDataModel([]);
  this.files_.addEventListener('splice', this.handleOnFilesChanged_.bind(this));

  this.title_ = document.querySelector('.header-title');

  this.loginDiv_ = document.querySelector('.login-div');
  this.loginInput_ = document.getElementById('login-input');
  this.passwordInput_ = document.getElementById('password-input');
  this.captchaRow_ = document.querySelector('.captcha-row');
  this.captchaImage_ = document.querySelector('.captcha-row img');
  this.captchaInput_ = document.getElementById('captcha-input');
  this.loginFailure_ = document.querySelector('.login-failure');
  this.loginButton_ = document.querySelector('.login-button');
  this.loginButton_.addEventListener('click',
      this.handleOnLoginClicked_.bind(this));

  var buttons = document.querySelectorAll('.cancel-button');
  for (var i = 0; i < buttons.length; i++) {
    buttons[i].addEventListener('click', this.close_.bind(this));
  }

  this.albumDiv_ = document.querySelector('.album-div');
  this.filesCountSpan_ = document.getElementById('files-count-span');

  this.albumSelect_ = document.getElementById('album-select');
  this.albumSelect_.addEventListener('change',
      this.handleOnAlbumSelected_.bind(this));
  this.albumTitleInput_ = document.getElementById('album-title-input');
  this.albumTitleInput_.addEventListener('change',
      this.handleOnAlbumTitleChanged_.bind(this));
  this.albumLocationInput_ = document.getElementById('album-location-input');
  this.albumLocationInput_.addEventListener('change',
      this.handleOnAlbumLocationChanged_.bind(this));
  this.albumDescriptionTextarea_ =
      document.getElementById('album-description-textarea');
  this.albumDescriptionTextarea_.addEventListener('change',
      this.handleOnAlbumDescriptionChanged_.bind(this));

  this.previewGrid_ = document.querySelector('.preview-grid');
  cr.ui.Grid.decorate(this.previewGrid_);
  this.previewGrid_.itemConstructor = PreviewItem;
  this.previewGrid_.dataModel = this.files_;
  this.previewGrid_.selectionModel = new cr.ui.ListSingleSelectionModel();

  this.uploadButton_ = document.querySelector('.upload-button');
  this.uploadButton_.addEventListener('click',
      this.handleOnUploadClicked_.bind(this));
  document.querySelector('.logout-button').addEventListener('click',
      this.handleOnLogoutClicked_.bind(this));

  // Login page is shown first.
  this.showLogin_();
  this.client_ = chrome.extension.getBackgroundPage().bg.client;
  // If user logged in before, skip the login page.
  if (this.client_.authorized) {
    this.loginCallback_('success');
  }
}

UploadPage.prototype = {
  /**
   * Adds more files to upload.
   * @param {Array.<picasa.LocalFile>} files The files to add.
   */
  addFiles: function(files) {
    for (var i = 0; i < files.length; i++) {
      this.files_.push(files[i]);
    }
  },

  /**
   * Loads files to upload from background page.
   */
  loadFilesFromBackgroundPage_: function() {
    var newFiles = chrome.extension.getBackgroundPage().bg.getNewFiles();
    this.addFiles(newFiles);
  },

  /**
   * @return {boolean} Whether upload should be enabled.
   */
  canUpload_: function() {
    return (this.albumTitleInput_.value != '') &&
           (this.files_.length > 0) && this.client_.authorized;
  },

  /**
   * Sets enabled attribute to element.
   * @param {HTMLElement} elt Element to set enabled on.
   * @param {boolean} enabled Whether element should be enabled or disabled.
   */
  setEnabled_: function(elt, enabled) {
    if (enabled) {
      elt.removeAttribute('disabled');
    } else {
      elt.setAttribute('disabled', 'disabled');
    }
  },

  /**
   * Enables/disables upload button as needed.
   */
  checkUploadButtonEnabled_: function() {
    this.setEnabled_(this.uploadButton_, this.canUpload_());
  },

  /**
   * Shows login page.
   */
  showLogin_: function() {
    this.title_.textContent = 'Sign in to Picasa';
    this.loginDiv_.classList.remove('invisible');
    this.albumDiv_.classList.add('invisible');
    this.captchaRow_.classList.add('invisible');
  },

  /**
   * Shows the number of upload files.
   */
  showFilesCount_: function() {
    this.filesCountSpan_.textContent = '' + this.files_.length;
  },

  /**
   * Shows the "choose album" page.
   */
  showChooseAlbum_: function() {
    this.title_.textContent = 'Choose an Album';
    this.checkUploadButtonEnabled_();
    this.loginDiv_.classList.add('invisible');
    this.albumDiv_.classList.remove('invisible');
    this.showFilesCount_();
  },

  /**
   * Event handler for files list changed.
   * @param {Event} e Event.
   */
  handleOnFilesChanged_: function(e) {
    this.showFilesCount_();
    this.checkUploadButtonEnabled_();
  },

  /**
   * Callback from picasa client after login.
   * @param {string} status Login status: success, failure or captcha.
   */
  loginCallback_: function(status) {
    this.setEnabled_(this.loginButton_, true);
    if (status == 'success') {
      this.loginFailure_.classList.add('invisible');
      this.showChooseAlbum_();
      this.loadFilesFromBackgroundPage_();
      this.client_.getAlbums(this.getAlbumsCallback_.bind(this));
    } else if (status == 'captcha') {
      this.captchaRow_.classList.remove('invisible');
      this.captchaImage_.setAttribute('src', this.client_.captchaUrl);
    } else if (status == 'failure') {
      this.loginFailure_.classList.remove('invisible');
    }
  },

  /**
   * Event handler for login button clicked.
   * @param {Event} e Event.
   */
  handleOnLoginClicked_: function(e) {
    this.setEnabled_(this.loginButton_, false);
    var password = this.passwordInput_.value;
    this.passwordInput_.value = '';
    var captcha = this.client_.captchaUrl ? this.captchaInput_.value : null;
    this.client_.login(this.loginInput_.value, password,
        this.loginCallback_.bind(this), captcha);
  },

  /**
   * Callback from picasa client after getting albums list.
   * @param {Array.<picasa.Album>} albums Albums list.
   */
  getAlbumsCallback_: function(albums) {
    // Adding fake album with id=null.
    this.client_.albums.push(
        new picasa.Album(null, 'Create New Album', '', '', ''));
    albums = this.client_.albums;
    this.albumSelect_.options.length = 0;
    for (var album, i = 0; album = albums[i]; i++) {
      this.albumSelect_.options[i] = new Option(album.title, i);
    }
    this.albumSelect_.selectedIndex = albums.length - 1;
    albums[albums.length - 1].title = '';
    this.handleOnAlbumSelected_(null);
  },

  /**
   * Event handler for logout button clicked.
   * @param {Event} e Event.
   */
  handleOnLogoutClicked_: function(e) {
    this.client_.logout();
    this.showLogin_();
  },

  /**
   * Event handler for album selection changed.
   * @param {Event} e Event.
   */
  handleOnAlbumSelected_: function(e) {
    var album = this.client_.albums[this.albumSelect_.selectedIndex];
    var enabled = album.id == null;
    this.setEnabled_(this.albumTitleInput_, enabled);
    this.setEnabled_(this.albumLocationInput_, enabled);
    this.setEnabled_(this.albumDescriptionTextarea_, enabled);
    this.albumTitleInput_.value = album.title;
    this.albumLocationInput_.value = album.location;
    this.albumDescriptionTextarea_.value = album.description;
    this.checkUploadButtonEnabled_();
  },

  /**
   * Event handler for upload button clicked.
   * @param {Event} e Event.
   */
  handleOnUploadClicked_: function(e) {
    this.setEnabled_(this.uploadButton_, false);
    var album = this.client_.albums[this.albumSelect_.selectedIndex];
    if (album.id == null) {
      this.client_.createAlbum(album, this.createAlbumCallback_.bind(this));
    } else {
      this.createAlbumCallback_(album);
    }
  },

  /**
   * Callback from picasa client after album created.
   * @param {Event} e Event.
   */
  createAlbumCallback_: function(album) {
    if (!album) {
      this.setEnabled_(this.uploadButton_, true);
      alert('Failed to create album.\nTry again.');
      return;
    }

    var files = [];
    for (var i = 0; i < this.files_.length; i++) {
      files.push(this.files_.item(i));
    }

    chrome.extension.getBackgroundPage().bg.uploadFiles(files, album);
    this.close_();
  },

  /**
   * Event handler for album title changed.
   * @param {Event} e Event.
   */
  handleOnAlbumTitleChanged_: function(e) {
    var album = this.client_.albums[this.albumSelect_.selectedIndex];
    album.title = this.albumTitleInput_.value;
    this.checkUploadButtonEnabled_();
  },

  /**
   * Event handler for album location changed.
   * @param {Event} e Event.
   */
  handleOnAlbumLocationChanged_: function(e) {
    var album = this.client_.albums[this.albumSelect_.selectedIndex];
    album.location = this.albumLocationInput_.value;
  },

  /**
   * Event handler for album description changed.
   * @param {Event} e Event.
   */
  handleOnAlbumDescriptionChanged_: function(e) {
    var album = this.client_.albums[this.albumSelect_.selectedIndex];
    album.description = this.albumDescriptionTextarea_.value;
  },

  /**
   * Closes the upload page.
   */
  close_: function() {
    window.close();
  }
};

var uploadPage = new UploadPage();
