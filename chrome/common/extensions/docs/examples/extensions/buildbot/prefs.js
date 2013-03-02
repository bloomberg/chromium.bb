// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Prefs() {
  this.defaults_ = {prefs: {use_notifications: false,
                            try_job_username: ""}};
}

Prefs.prototype = {
  get_: function(key, callback) {
    chrome.storage.sync.get(this.defaults_,
        function (storage) {
          callback(storage.prefs[key]);
        });
  },

  set_: function(key, value) {
    chrome.storage.sync.get(this.defaults_,
        function (storage) {
          storage.prefs[key] = value;
          chrome.storage.sync.set(storage);
        });
  },

  getUseNotifications: function(callback) {
    this.get_("use_notifications", callback);
  },

  setUseNotifications: function(use_notifications) {
    this.set_("use_notifications", use_notifications);
  },

  getTryJobUsername: function(callback) {
    this.get_("try_job_username", callback);
  },

  setTryJobUsername: function(try_job_username) {
    this.set_("try_job_username", try_job_username);
  }
};
