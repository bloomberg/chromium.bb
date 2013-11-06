// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

aM = null;

function moduleLoad() {
  hideStatus();
  init();
  animate();
  NaClAMBulletInit();
  loadJenga20();

  document.querySelector('#curtain').classList.add('lift');
}

function moduleLoadError() {
  updateStatus('Load failed.');
}

function moduleLoadProgress(event) {
  $('progress').style.display = 'block';

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

function moduleCrash(event) {
  if (naclModule.exitStatus == -1) {
    updateStatus('CRASHED');
  } else {
    updateStatus('EXITED [' + naclModule.exitStatus + ']');
  }
}

function updateStatus(opt_message) {
  document.querySelector('#curtain').classList.remove('lift');

  var statusField = $('statusField');
  if (statusField) {
    statusField.style.display = 'block';
    statusField.textContent = opt_message;
  }
}

function hideStatus() {
  $('statusField').style.display = 'none';
  $('progress').style.display = 'none';
}

function pageDidLoad() {
  updateStatus('Loading...');
  console.log('started');

  aM = new NaClAM('NaClAM');
  aM.enable();

  var embedWrap = document.createElement('div');
  embedWrap.addEventListener('load', moduleLoad, true);
  embedWrap.addEventListener('error', moduleLoadError, true);
  embedWrap.addEventListener('progress', moduleLoadProgress, true);
  embedWrap.addEventListener('crash', moduleCrash, true);
  document.body.appendChild(embedWrap);

  var embed = document.createElement('embed');
  embed.setAttribute('id', 'NaClAM');
  embed.setAttribute('width', '0');
  embed.setAttribute('height', '0');
  embed.setAttribute('type', 'application/x-pnacl');
  embed.setAttribute('src', 'http://commondatastorage.googleapis.com/gonacl/demos/publish/233080/bullet/NaClAMBullet.nmf');
  embedWrap.appendChild(embed);
}

window.addEventListener("load", pageDidLoad, false);
