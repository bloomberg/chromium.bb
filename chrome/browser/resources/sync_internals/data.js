// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
var dumpToTextButton = document.getElementById('dump-to-text');
var dataDump = document.getElementById('data-dump');
dumpToTextButton.addEventListener('click', function(event) {
  // TODO(akalin): Add info like Chrome version, OS, date dumped, etc.

  var data = '';
  data += '======\n';
  data += 'Status\n';
  data += '======\n';
  data += JSON.stringify(chrome.sync.aboutInfo, null, 2);
  data += '\n';
  data += '\n';

  data += '=============\n';
  data += 'Notifications\n';
  data += '=============\n';
  data += JSON.stringify(chrome.sync.notifications, null, 2);
  data += '\n';
  data += '\n';

  data += '===\n';
  data += 'Log\n';
  data += '===\n';
  data += JSON.stringify(chrome.sync.log.entries, null, 2);
  data += '\n';

  dataDump.textContent = data;
});
})();
