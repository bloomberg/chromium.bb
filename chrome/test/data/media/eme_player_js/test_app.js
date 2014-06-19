// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TestApp is responsible for starting playback on the page. It selects the
// suitable player to use based on key system and other test variables.
var TestApp = new function() {
  this.video_ = null;
}

TestApp.loadPlayer = function() {
  if (this.video_) {
    Utils.timeLog('Delete old video tag.');
    this.video_.pause();
    this.video_.remove();
    delete(this.video_);
  }

  this.video_ = document.createElement('video');
  this.video_.controls = true;
  this.video_.preload = true;
  this.video_.width = 848;
  this.video_.height = 480;

  // Check if the key system is supported.
  var videoPlayer = this.getPlayer();
  if (!videoPlayer) {
    Utils.timeLog('Cannot create a media player.');
    return;
  }
  Utils.timeLog('Using ' + videoPlayer.constructor.name);
  var videoSpan = document.getElementById(VIDEO_ELEMENT_ID);
  if (videoSpan)
    videoSpan.appendChild(this.video_);
  else
    document.body.appendChild(this.video_);
  videoPlayer.init(this.video_);

  if (TestConfig.runFPS)
    FPSObserver.observe(this.video_);

  this.video_.play();
  return this.video_;
};

TestApp.getPlayer = function() {
  // Update keySystem if using prefixed Clear Key since it is not available as a
  // separate key system to choose from; however it can be set in URL query.
  var usePrefixedEME = TestConfig.usePrefixedEME;
  if (TestConfig.keySystem == CLEARKEY && usePrefixedEME)
    TestConfig.keySystem = PREFIXED_CLEARKEY;
  var keySystem = TestConfig.keySystem;

  switch (keySystem) {
    case WIDEVINE_KEYSYSTEM:
      if (usePrefixedEME)
        return new PrefixedWidevinePlayer();
      return new WidevinePlayer();
    case PREFIXED_CLEARKEY:
       return new PrefixedClearKeyPlayer();
    case EXTERNAL_CLEARKEY:
    case CLEARKEY:
      if (usePrefixedEME)
        return new PrefixedClearKeyPlayer();
      return new ClearKeyPlayer();
    case FILE_IO_TEST_KEYSYSTEM:
      if (usePrefixedEME)
        return new FileIOTestPlayer();
    default:
      Utils.timeLog(keySystem + ' is not a known key system');
      if (usePrefixedEME)
        return new PrefixedClearKeyPlayer();
      return new ClearKeyPlayer();
  }
};
