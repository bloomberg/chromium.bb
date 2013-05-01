// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var itrMax = 1000;
var itrCount = 0;
var itrSend = new Float64Array(itrMax);
var itrNaCl = new Float64Array(itrMax);
var itrRecv = new Float64Array(itrMax);
var delay = 0;

function attachListeners() {
  document.getElementById('start').addEventListener('click',
      startTest);
  count_pp = document.getElementById('count')
  count_pp.textContent = itrMax;
}

function startTest() {
  if (common.naclModule) {
    var startButton = document.getElementById('start');
    startButton.disabled = true;

    var delayControl = document.getElementById('delay');
    delay = parseInt(delayControl.value, 10);

    common.updateStatus('Running Test');
    itrCount = 0;
    itrSend[0] = (new Date()).getTime();
    common.naclModule.postMessage(delay);
  }
}

function setStats(nacl, compute, total) {
  var statNaCl = document.getElementById('NaCl');
  var statRound = document.getElementById('Round');
  var statAll = document.getElementById('Total');

  statNaCl.textContent = (nacl / itrMax) + ' ms';
  statRound.textContent = (compute / itrMax) + ' ms';
  statAll.textContent = (total / itrMax) + ' ms';
}

// Called by the common.js module.
function handleMessage(message_event) {
  // Convert NaCl Seconds elapsed to MS
  itrNaCl[itrCount] = message_event.data * 1000.0;
  itrRecv[itrCount] = (new Date()).getTime();
  itrCount++;

  if (itrCount == itrMax) {
    common.updateStatus('Test Finished');
    var startButton = document.getElementById('start');
    startButton.disabled = false;

    var naclMS = 0.0;
    var computeMS = 0.0;
    for (var i=0; i < itrMax; i++) {
      naclMS += itrNaCl[i];
      computeMS += itrRecv[i] - itrSend[i];
    }

    setStats(naclMS, computeMS, itrRecv[itrMax - 1] - itrSend[0]);
  } else {
    itrSend[itrCount] = (new Date()).getTime();
    common.naclModule.postMessage(delay);
  }
}
