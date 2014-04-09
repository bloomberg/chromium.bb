// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function toggleHelpBox() {
  var helpBoxOuter = document.getElementById('help-box-outer');
  helpBoxOuter.classList.toggle('hidden');
  var moreLessButton = document.getElementById('more-less-button');
  if (helpBoxOuter.classList.contains('hidden')) {
    moreLessButton.innerText = moreLessButton.moreText;
  } else {
    moreLessButton.innerText = moreLessButton.lessText;
  }
}

function diagnoseErrors() {
  var extension_id = "idddmepepmjcgiedknnmlbadcokidhoa";
  var diagnose_frame = document.getElementById('diagnose-frame');
  diagnose_frame.innerHTML =
      '<iframe src="chrome-extension://' + extension_id +
      '/index.html"></iframe>';
}

// Subframes use a different layout but the same html file.  This is to make it
// easier to support platforms that load the error page via different
// mechanisms (Currently just iOS).
if (window.top.location != window.location)
  document.documentElement.setAttribute('subframe', '');

// Re-renders the error page using |strings| as the dictionary of values.
// Used by NetErrorTabHelper to update DNS error pages with probe results.
function updateForDnsProbe(strings) {
  i18nTemplate.process(document, strings);
  var context = new JsEvalContext(strings);
  jstProcess(context, document.getElementById('t'));
}

// Given the classList property of an element, adds an icon class to the list
// and removes the previously-
function updateIconClass(classList, newClass) {
  var oldClass;

  if (classList.hasOwnProperty('last_icon_class')) {
    oldClass = classList['last_icon_class']
    if (oldClass == newClass)
      return;
  }

  classList.add(newClass);
  if (oldClass !== undefined)
    classList.remove(oldClass);

  classList['last_icon_class'] = newClass;
}

<if expr="is_macosx or is_ios or is_linux or is_android">
// Re-orders buttons. Used on Mac, Linux, and Android, where reload should go
// on the right.
function swapButtonOrder() {
  reloadButton = document.getElementById('reload-button');
  moreLessButton = document.getElementById('more-less-button');
  reloadButton.parentNode.insertBefore(moreLessButton, reloadButton);
}
document.addEventListener("DOMContentLoaded", swapButtonOrder);
</if>
