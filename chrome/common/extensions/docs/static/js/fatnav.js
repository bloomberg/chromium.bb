// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Adds toggle controls to the fat navbar.
 */

(function() {
var isTouch = (('ontouchstart' in window) || (navigator.msMaxTouchPoints > 0));
var isLargerThanPhone = window.matchMedia('screen and (min-width: 580px)').matches;

var fatNav = document.querySelector('#fatnav');
var search = document.querySelector('#search');

function hideActive(parentNode) {
  //parentNode.classList.remove('active');

  [].forEach.call(parentNode.querySelectorAll('.active'), function(el, i) {
    el.classList.remove('active');
  });
}

// Clicking outside the fatnav.
document.body.addEventListener('click', function(e) {
  hideActive(fatNav);
});


// Fatnav activates onclick and closes on mouseleave.
var pillars = document.querySelectorAll('.pillar');
[].forEach.call(pillars, function(pillar, i) {
  pillar.addEventListener('click', function(e) {
    if (e.target.classList.contains('toplevel')) {
      e.stopPropagation(); // prevent body handler from being called.
      var wasAlreadyOpen = this.classList.contains('active');
      hideActive(fatNav); // de-activate other fatnav items.
      wasAlreadyOpen ? this.classList.remove('active') :
                       this.classList.add('active');
    }
  });

  pillar.addEventListener('mouseleave', function(e) {
    if (!e.target.classList.contains('pillar') ||
        e.toElement.classList.contains('expandee')) {
      return;
    }

    if (e.toElement != fatNav && !e.toElement.classList.contains('pillar') &&
        e.toElement != search) {
      hideActive(fatNav);
    }
  });

});

if (isLargerThanPhone) {
  search.addEventListener('click', function(e) {
    e.stopPropagation();

    // Only toggle if magnifying glass is clicked.
    if (e.target.localName == 'img') {
      var wasAlreadyOpen = this.classList.contains('active');
      hideActive(fatNav); // de-activate other fatnav items.
      wasAlreadyOpen ? this.classList.remove('active') :
                       this.classList.add('active');
      if (!wasAlreadyOpen) {
        var searchField = document.getElementById('chrome-docs-cse-input');
        var cse = google && google.search && google.search.cse && google.search.cse.element.getElement('results') || null;
        if (cse)
          cse.clearAllResults();
        searchField.select();
        searchField.focus();
      }
    }
  });

} else {
  var mobileNavCollasper = document.querySelector('#topnav .collase-icon');
  mobileNavCollasper.addEventListener('click', function(e) {
    e.stopPropagation();
    fatNav.classList.toggle('active');
    this.classList.toggle('active');
  });
}

if (!isTouch) {
  // Hitting ESC hides fatnav menus.
  document.body.addEventListener('keydown', function(e) {
    if (e.keyCode == 27) { // ESC
      hideActive(fatNav);
    }
  });
}

})();
