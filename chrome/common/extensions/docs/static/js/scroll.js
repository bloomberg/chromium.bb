// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scroll handling.
//
// Switches the sidebar between floating on the left and position:fixed
// depending on whether it's scrolled into view, and manages the scroll-to-top
// button: click logic, and when to show it.
(function() {

var sidebar = document.getElementById('gc-sidebar');
var scrollToTop = document.getElementById('scroll-to-top');
var offsetTop = sidebar.offsetTop;

window.addEventListener('scroll', function() {
  // Obviously, this code executes every time the window scrolls, so avoid
  // putting things in here.
  var floatSidebar = false;
  var showScrollToTop = false;

  if (window.scrollY > offsetTop) {
    // Scrolled past the top of the sidebar.
    if (window.innerHeight >= sidebar.offsetHeight) {
      // The whole sidebar fits in the window. Make it always visible.
      floatSidebar = true;
    } else {
      // Whole sidebar doesn't fit, so show the scroll-to-top button instead.
      showScrollToTop = true;
    }
  }

  sidebar.classList.toggle('floating', floatSidebar);
  scrollToTop.classList.toggle('hidden', !showScrollToTop);
});

scrollToTop.addEventListener('click', function() {
  window.scrollTo(0, 0);
});

}());
