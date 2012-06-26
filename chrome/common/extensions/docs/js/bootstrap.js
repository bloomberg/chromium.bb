// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var fileXHREnabled = function() {
  var xhr = new XMLHttpRequest();
  try {
    xhr.onreadystatechange = function() {};
    xhr.onerror = function() {};
    xhr.open("GET", "nothing.xml", true);
    xhr.send(null);
  } catch (e) {
    return false;
  }
  
  xhr.abort();
  return true;
}();

var officialURL = (function() {
  var candidates = [
    'http://code.google.com/chrome/extensions/',
    'http://developer.chrome.com/',

    // TODO(aa): Remove this one once developer.chrome.com is live.
    'http://chrome-apps-doc.appspot.com/'
  ];
  for (var i = 0, url; url = candidates[i]; i++) {
    if (location.href.indexOf(candidates[i]) == 0)
      return candidates[i];
  }
  return '';
})();

function getCurrentBranch() {
  var branchNames = ['beta', 'dev', 'trunk'];

  if (!officialURL)
    return '';

  var branch = location.href.substring(
      officialURL.length,
      location.href.indexOf('/', officialURL.length));
  if (branchNames.indexOf(branch) == -1)
    return 'stable';

  return branch;
}

// Regenerate page if we are passed the "?regenerate" search param
// or if the user-agent is chrome AND the document is being served
// from the file:/// scheme.
if (window.location.search == "?regenerate" ||
    (navigator.userAgent.indexOf("Chrome") > -1) &&
    (window.location.href.match("^file:")) &&
    fileXHREnabled)    {
  
  // Hide body content initially to minimize flashing.
  document.write('<style id="hider" type="text/css">');
  document.write('body { display:none!important; }');
  document.write('</style>');

  window.onload = window.renderPage;

  window.postRender = function() {
    var elm = document.getElementById("hider");
    elm.parentNode.removeChild(elm);

    // Since populating the page is done asynchronously, the DOM doesn't exist
    // when the browser tries to resolve any #anchors in the URL. So we reset
    // the URL once we're done, which forces the browser to scroll to the anchor
    // as it normally would.
    if (location.hash.length > 1)
      location.href = location.href;
  }
} else if ((navigator.userAgent.indexOf("Chrome") > -1) &&
           (window.location.href.match("^file:")) &&
            !fileXHREnabled) {
  window.onload = function() {
    // Display the warning to use the --allow-file-access-from-files.
    document.getElementById("devModeWarning").style.display = "block";
  }
} else {
  window.onload = function() {
    if (location.pathname.split('/').reverse()[1] != 'apps') {
      var currentBranch = getCurrentBranch();
      if (currentBranch == '') {
        document.getElementById('unofficialWarning').style.display = 'block';
        document.getElementById('goToOfficialDocs').onclick = function() {
          location.href = officialURL;
        };
      } else if (currentBranch != 'stable') {
        document.getElementById("branchName").textContent =
            currentBranch.toUpperCase();
        document.getElementById('branchWarning').style.display = 'block';
        document.getElementById('branchChooser').onchange = function() {
          location.href = officialURL + this.value + "/";
        };
      }
      if (currentBranch != 'stable' && currentBranch != 'beta') {
        var warning = document.getElementById('eventPageWarning');
        if (warning)
          warning.style.display = 'block';
      }
    }
  }
}
