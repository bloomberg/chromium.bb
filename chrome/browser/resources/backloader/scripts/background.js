// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function Loader(pages) {
  this.pages_ = pages;
  this.pagesLoaded_ = false;
  this.loadedCount_ = false;
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
  maxDelayMS_: 10000,

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
                             {delayInMinutes: this.DELAY_IN_MINUTES});
        window.close();
        return;
      }
    }.bind(this));
  },

  onAlarm_: function(alarm) {
    if (alarm.name != this.ALARM_NAME)
      return;

    this.preparePages_();
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

  // Find an extension in the |list| with matching extension |id|.
  getExtensionById_: function(list, id) {
    for (var i = 0; i < list.length; i++) {
      if (list[i].id == id)
        return list[i];
    }
    return null;
  },

  preparePages_: function() {
    if (this.pagesLoaded_)
      return;

    window.addEventListener('message', this.onMessage_.bind(this), false);
    chrome.management.getAll(function(list) {
      // Get total count first.
      var pagesToLoad = [];
      for (var i = 0; i < this.pages_.length; i++) {
        var page = this.pages_[i];
        if (page.oneTime && page.initialized)
          continue;

        var extension = this.getExtensionById_(list, page.extensionId);
        if (!extension || !extension.enabled) {
          page.initialized = true;
          continue;
        }

        page.initialized = true;
        if (page.unloadDelayMS > this.maxDelayMS_)
          this.maxDelayMS_ = page.unloadDelayMS;

        pagesToLoad.push(page);
      }
      this.loadFrames_(pagesToLoad);
      this.pagesLoaded_ = true;
    }.bind(this));
  },

  loadFrames_: function(pages) {
    this.load_target_ = pages.length;
    for (var i = 0; i < pages.length; i++) {
      this.loadLuncherFrame_(i, pages[i].pageUrl);
    }
  },

  incrementLoadCounter_: function() {
    this.loadedCount_++;
    if (this.loadedCount_ < this.load_target_)
      return;

    // Delay closing.
    setInterval(function() {
      window.close();
    }.bind(this), this.maxDelayMS_);
  },

  loadLuncherFrame_: function(index, pageUrl) {
    var iframe = document.createElement('iframe');
    iframe.onload = function() {
      this.incrementLoadCounter_();
    }.bind(this);
    iframe.src = pageUrl;
    iframe.name = 'frame_' + index;
    document.body.appendChild(iframe);
  }
};

Loader.getInstance().initialize();
