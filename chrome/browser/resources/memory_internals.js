// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function requestProcessList() {
  chrome.send('requestProcessList');
}

function dumpProcess(pid) {
  chrome.send('dumpProcess', [pid]);
}

function returnProcessList(processList) {
  var proclist = $('proclist');
  proclist.innerText = '';
  for (let proc of processList) {
    /** @const */ var row = document.createElement('div');
    row.className = 'procrow';

    var description = document.createTextNode(proc[1] + ' ');
    row.appendChild(description);

    var button = document.createElement('button');
    button.innerText = '[dump]';
    button.className = 'button';
    let proc_id = proc[0];
    button.onclick = () => dumpProcess(proc_id);
    row.appendChild(button);

    proclist.appendChild(row);
  }
}

// Get data and have it displayed upon loading.
document.addEventListener('DOMContentLoaded', requestProcessList);

/* For manual testing.
function fakeResults() {
  returnProcessList([
    [ 11234, "Process 11234 [Browser]" ],
    [ 11235, "Process 11235 [Renderer]" ],
    [ 11236, "Process 11236 [Renderer]" ]]);
}
document.addEventListener('DOMContentLoaded', fakeResults);
*/
