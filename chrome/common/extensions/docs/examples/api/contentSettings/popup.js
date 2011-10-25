// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var incognito;
var url;

function settingChanged() {
  var type = this.id;
  var setting = this.value;
  var pattern = /^file:/.test(url) ? url : url.replace(/\/[^\/]*?$/, '/*');
  console.log(type+' setting for '+pattern+': '+setting);
  chrome.contentSettings[type].set({
        'primaryPattern': pattern,
        'setting': setting,
        'scope': (incognito ? 'incognito_session_only' : 'regular')
      });
}

document.addEventListener('DOMContentLoaded', function () {
  chrome.tabs.getSelected(undefined, function(tab) {
    incognito = tab.incognito;
    url = tab.url;
    var types = ['cookies', 'images', 'javascript', 'plugins', 'popups',
                 'notifications'];
    types.forEach(function(type) {
      chrome.contentSettings[type].get({
            'primaryUrl': url,
            'incognito': incognito
          },
          function(details) {
            document.getElementById(type).value = details.setting;
          });
    });
  });

  var selects = document.querySelectorAll('select');
  for (var i = 0; i < selects.length; i++) {
    selects[i].addEventListener('change', settingChanged);
  }
});

