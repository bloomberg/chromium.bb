// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

picasa = {}

/**
 * LocalFile constructor.
 *
 * LocalFile object represents a file to be uploaded.
 */
picasa.LocalFile = function(file) {
  this.file_ = file;
  this.caption = file.name;

  this.dataUrl_ = null;
  this.mime_ = file.type;
};

picasa.LocalFile.prototype = {
  /**
   * Reads data url from local file to show in img element.
   * @param {Function} callback Callback.
   */
  readData_: function(callback) {
    if (this.dataUrl_) {
      callback.call(this);
      return;
    }

    var reader = new FileReader();
    function onLoadCallback(e) {
      this.dataUrl_ = e.target.result;
      this.mime_ = this.dataUrl_.substring(0, this.dataUrl_.indexOf(';base64'));
      this.mime_ = this.mime_.substr(5);  // skip 'data:'
      callback.call(this);
    }
    reader.onload =  onLoadCallback.bind(this);
    reader.readAsDataURL(this.file_);
  },

  showInImage: function(img) {
    if (this.dataUrl_) {
      img.setAttribute('src', this.dataUrl_);
      return;
    }

    this.readData_(function() {
      img.setAttribute('src', this.dataUrl_);
    });
  },

  /**
   * @return {string} Mime type of the file.
   */
  get mimeType() {
    return this.mime_;
  }
};


/**
 * Album constructor.
 *
 * Album object stores information about picasa album.
 */
picasa.Album = function(id, title, location, description, link) {
  this.id = id;
  this.title = title;
  this.location = location;
  this.description = description;
  this.link = link;
};


/**
 * Client constructor.
 *
 * Client object stores user credentials and gets from and sends to picasa
 * web server.
 */
picasa.Client = function() {
};


picasa.Client.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * User credentials.
   * @type {string}
   * @private
   */
  authToken_: null,

  /**
   * User id.
   * @type {string}
   * @private
   */
  userID_: null,

  /**
   * List of user albums.
   * @type {Array.<picasa.Album>}
   * @private
   */
  albums_: null,

  /**
   * Url for captcha challenge, if required.
   * @type {string}
   * @private
   */
  captchaUrl_: null,

  /**
   * Captcha toekn, if required.
   * @type {string}
   * @private
   */
  captchaToken_: null,

  /**
   * Whether client is already authorized.
   * @type {boolean}
   */
  get authorized() {
    return !!this.authToken_;
  },

  /**
   * User id.
   * @type {string}
   */
  get userID() {
    return this.userID_ || '';
  },

  /**
   * List of albums.
   * @type {Array.<picasa.Album>}
   */
  get albums() {
    return this.albums_ || [];
  },

  /**
   * Captcha url to show to user, if needed.
   * @type {string}
   */
  get captchaUrl() {
    return this.captchaUrl_;
  },

  /**
   * Get user credential for picasa web server.
   * @param {string} login User login.
   * @param {string} password User password.
   * @param {Function(string)} callback Callback, which is passed 'status'
   *     parameter: either 'success', 'failure' or 'captcha'.
   * @param {?string=} opt_captcha Captcha answer, if was required.
   */
  login: function(login, password, callback, opt_captcha) {
    function xhrCallback(xhr) {
      if (xhr.status == 200) {
        this.authToken_ = this.extractResponseField_(xhr.responseText, 'Auth');
       this.userID_ = login;
        callback('success');
      } else {
        var response = xhr.responseText;
        var error = this.extractResponseField_(response, 'Error');
        if (error == 'CaptchaRequired') {
          this.captchaToken_ = this.extractResponseField_(response,
              'CaptchaToken');
          // Captcha url should prefixed with this.
          this.captchaUrl_ = 'http://www.google.com/accounts/' +
              this.extractResponseField_(response, 'CaptchaUrl');
          callback('captcha');
          return;
        }
        callback('failure');
      }
    }

    var content = 'accountType=HOSTED_OR_GOOGLE&Email=' + login +
        '&Passwd=' + password + '&service=lh2&source=ChromeOsPWAUploader';
    if (opt_captcha && this.captchaToken_) {
      content += '&logintoken=' + this.captchaToken_;
      content += '&logincaptcha=' + opt_captcha;
    }
    this.sendRequest('POST', 'https://www.google.com/accounts/ClientLogin',
        {'Content-type': 'application/x-www-form-urlencoded'},
        content,
        xhrCallback.bind(this));
  },

  /**
   * Logs out.
   */
  logout: function() {
    this.authToken_ = null;
    this.userID_ = null;
    this.captchaToken_ = null;
    this.captchatUrl_ = null;
  },

  /**
   * Extracts text field from text response.
   * @param {string} response The response.
   * @param {string} field Field name to extract value of.
   * @return {?string} Field value or null.
   */
  extractResponseField_: function(response, field) {
    var lines = response.split('\n');
    field += '=';
    for (var line, i = 0; line = lines[i]; i++) {
      if (line.indexOf(field) == 0) {
        return line.substr(field.length);
      }
    }
    return null;
  },

  /**
   * Sends request to web server.
   * @param {string} method Method to use (GET or POST).
   * @param {string} url Request url.
   * @param {Object.<string, string>} headers Request headers.
   * @param {*} body Request body.
   * @param {Function(XMLHttpRequest)} callback Callback.
   */
  sendRequest: function(method, url, headers, body, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4) {
        callback(xhr);
      }
    };
    xhr.open(method, url, true);
    if (headers) {
      for (var header in headers) {
        if (headers.hasOwnProperty(header)) {
          xhr.setRequestHeader(header, headers[header]);
        }
      }
    }
    xhr.send(body);
    return xhr;
  },

  /**
   * Gets the feed from web server and parses it. Appends user credentials.
   * @param {string} url Feed url.
   * @param {Function(*)} callback Callback.
   */
  getFeed: function(url, callback) {
    var headers = {'Authorization': 'GoogleLogin auth=' + this.authToken_};
    this.sendRequest('GET', url + '?alt=json', headers, null, function(xhr) {
      if (xhr.status == 200) {
        var feed = JSON.parse(xhr.responseText);
        callback(feed);
      } else {
        callback(null);
      }
    });
  },

  /**
   * Posts the feed to web server. Appends user credentials.
   * @param {string} url Feed url.
   * @param {Object.<string, string>} headers Request headers.
   * @param {*} body Post body.
   * @param {Function(!string)} callback Callback taking response text or
   *     null in the case of failure.
   */
  postFeed: function(url, headers, body, callback) {
    headers['Authorization'] = 'GoogleLogin auth=' + this.authToken_;
    return this.sendRequest('POST', url, headers, body, function(xhr) {
      if (xhr.status >= 200 && xhr.status <= 202) {
        callback(xhr.responseText);
      } else {
        callback(null);
      }
    });
  },

  /**
   * Requests albums for the user and passes them to callback.
   * @param {Function(Array.<picasa.Album>)} callback Callback.
   */
  getAlbums: function(callback) {
    function feedCallback(feed) {
      feed = feed.feed;
      if (!feed.entry) {
        return;
      }
      this.albums_ = [];
      for (var entry, i = 0; entry = feed.entry[i]; i++) {
        this.albums_.push(this.albumFromEntry_(entry));
      }
      callback(this.albums_);
    }

    this.getFeed('https://picasaweb.google.com/data/feed/api/user/' +
        this.userID_, feedCallback.bind(this));
  },

  /**
   * Returns album object created from entry.
   * @param {*} entry The feed entry corresponding to album.
   * @return {picasa.Album} The album object.
   */
  albumFromEntry_: function(entry) {
    var altLink = '';
    for (var link, j = 0; link = entry.link[j]; j++) {
      if (link.rel == 'alternate') {
        altLink = link.href;
      }
    }
    return new picasa.Album(entry['gphoto$id']['$t'], entry.title['$t'],
        entry['gphoto$location']['$t'], entry.summary['$t'], altLink);
  },

  /**
   * Send request to create album.
   * @param {picasa.Album} album Album to create.
   * @param {Function(picasa.Album)} callback Callback taking updated album
   *     (for example, with created album id).
   */
  createAlbum: function(album, callback) {
    function postCallback(response) {
      if (response == null) {
        callback(null);
      } else {
        var entry = JSON.parse(response).entry;
        callback(this.albumFromEntry_(entry));
      }
    }

    var eol = '\n';
    var postData = '<entry xmlns="http://www.w3.org/2005/Atom"' + eol;
    postData += 'xmlns:media="http://search.yahoo.com/mrss/"' + eol;
    postData += 'xmlns:gphoto="http://schemas.google.com/photos/2007">' + eol;
    postData += '<title type="text">' + escape(album.title) + '</title>' + eol;
    postData += '<summary type="text">' + escape(album.description) +
        '</summary>' + eol;
    postData += '<gphoto:location>' + escape(album.location) +
        '</gphoto:location>' + eol;
    postData += '<gphoto:access>public</gphoto:access>';
    postData += '<category scheme="http://schemas.google.com/g/2005#kind" ' +
        'term="http://schemas.google.com/photos/2007#album"></category>' + eol;
    postData += '</entry>' + eol;

    var headers = {'Content-Type': 'application/atom+xml'};
    this.postFeed('https://picasaweb.google.com/data/feed/api/user/' +
        this.userID_ + '?alt=json', headers, postData, postCallback.bind(this));
  },

  /**
   * Uploads file to the given album.
   * @param {picasa.Album} album Album to upload to.
   * @param {picasa.LocalFile} file File to upload.
   * @param {Function(?string)} callback Callback.
   */
  uploadFile: function(album, file, callback) {
    var postData = file.file_;
    var headers = {
        'Content-Type': file.mimeType,
        'Slug': file.file_.name};
    return this.postFeed('https://picasaweb.google.com/data/feed/api/user/' +
        this.userID_ + '/albumid/' + album.id, headers, postData, callback);
  }
};
