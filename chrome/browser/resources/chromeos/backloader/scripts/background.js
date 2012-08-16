// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Loader(pages) {
  this.pages_ = pages;
  this.pages_loaded_ = false;
  this.loaded_count_ = false;
}

// Global instance.
Loader.instance_ = null;

// static.
Loader.getInstance = function() {
  if (!Loader.instance_) {
    if (!g_pages)
      console.log('Warning: Undefined g_pages.');
    Loader.instance_ = new Loader(g_pages);
  }

  return Loader.instance_;
};

Loader.prototype = {
  // Alarm name.
  ALARM_NAME: 'CrOSBkgLoaderAlarm',
  // Initial delay.
  DELAY_IN_MINUTES: 1,
  // Periodic wakeup delay.
  PERIOD_IN_MINUTES: 60,
  // Delayed closing of the background page once when all iframes are loaded.
  UNLOAD_DELAY_IN_MS: 10000,

  // Loader start up. Kicks off alarm initialization if needed.
  initialize: function() {
    if (!g_pages || !g_pages.length) {
      window.close();
      return;
    }

    chrome.alarms.onAlarm.addListener(this.onAlarm_.bind(this));
    // Check if this alarm already exists, if not then create it.
    chrome.alarms.get(this.ALARM_NAME, function(alarm) {
      if (!alarm) {
        chrome.alarms.create(this.ALARM_NAME,
                             {delayInMinutes: this.DELAY_IN_MINUTES,
                              periodInMinutes: this.PERIOD_IN_MINUTES});
        window.close();
        return;
      }
    }.bind(this));
  },

  onAlarm_: function(alarm) {
    this.loadSubPages_();
  },

  onMessage_: function(event) {
    var msg = event.data;
    if (msg.method == 'validate') {
      var reply_msg = {
          method: 'validationResults',
          os: 'ChromeOS'
      };
      event.source.postMessage(reply_msg, event.origin);
    } else {
      console.log('#### Loader.onMessage_: unknown message');
    }
  },

  loadSubPages_: function() {
    if (this.pages_loaded_)
      return;

    window.addEventListener('message', this.onMessage_.bind(this), false);
    for (i = 0; i < this.pages_.length; i++) {
      this.loadPage_(i, this.pages_[i]);
    }
    this.pages_loaded_ = true;
  },

  loadPage_: function(index, pageUrl) {
    var iframe = document.createElement('iframe');
    iframe.onload = function() {
      this.loaded_count_++;
      if (this.loaded_count_ < this.pages_.length)
        return;

      // Delay closing.
      setInterval(function() {
        window.close();
      }.bind(this), this.UNLOAD_DELAY_IN_MS);
    }.bind(this);
    iframe.src = pageUrl;
    iframe.name = 'frame_' + index;
    document.body.appendChild(iframe);
  }
};

Loader.getInstance().initialize();
