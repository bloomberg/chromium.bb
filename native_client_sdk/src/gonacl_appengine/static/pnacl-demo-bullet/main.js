// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

NaClAMModule = null;
aM = null;
statusText = 'NO-STATUS';

startTimes = [];
endTimes = [];

function moduleLoadStart() {
  startTimes.push(new Date().getTime());
}

function moduleLoad() {
  var bar = document.getElementById('progress');
  bar.value = 100;
  bar.max = 100;
  var bar_div = document.getElementById('progress_div');
  bar_div.innerHTML = 'Loaded!';

  endTimes.push(new Date().getTime());
  console.log('loaded in ' + getLastLoadTime());
}

function moduleLoadProgress(event) {
  var loadPercent = 0.0;
  var bar = document.getElementById('progress');
  if (event.lengthComputable && event.total > 0) {
    loadPercent = event.loaded / event.total * 100.0;
    bar.value = loadPercent;
    bar.max = 100;
  } else {
    // The total length is not yet known.
    loadPercent = -1.0;
  }
}

function getLastLoadTime() {
  return endTimes[endTimes.length - 1] - startTimes[startTimes.length - 1];
}

function pageDidLoad() {
  console.log('started');

  aM = new NaClAM('NaClAM');
  aM.enable();
  var AMDiv = document.getElementById('NaClAM');
  AMDiv.addEventListener("loadstart", moduleLoadStart, false);
  AMDiv.addEventListener("progress", moduleLoadProgress, false);
  AMDiv.addEventListener("load", moduleLoad, false);
  init();
  animate();
  NaClAMBulletInit();
  NaClAMBulletLoadScene(sceneDescription);
}

window.addEventListener("load", pageDidLoad, false);
