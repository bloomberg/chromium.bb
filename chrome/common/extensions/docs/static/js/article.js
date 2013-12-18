// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scroll handling.
//
// Switches the sidebar between floating on the left and position:fixed
// depending on whether it's scrolled into view, and manages the scroll-to-top
// button: click logic, and when to show it.
(function() {

var isLargerThanMobile = window.matchMedia('screen and (min-width: 580px)').matches;

var sidebar = document.querySelector('.inline-toc');
var articleBody = document.querySelector('[itemprop="articleBody"]');


function addPermalink(el) {
  el.classList.add('has-permalink');
  var id = el.id || el.textContent.toLowerCase().replace(' ', '-');
  el.insertAdjacentHTML('beforeend',
      '<a class="permalink" title="Permalink" href="#' + id + '">#</a>');
}

// Add permalinks to heading elements.
function addPermalinkHeadings(container) {
  if (container) {
    ['h2','h3','h4'].forEach(function(h, i) {
      [].forEach.call(container.querySelectorAll(h), addPermalink);
    });
  }
}

// Bomb out if we're not on an article page and have a TOC.
if (!(sidebar && articleBody)) {
  return;
}

if (isLargerThanMobile) {
  var toc = sidebar.querySelector('#toc');
  var offsetTop = sidebar.offsetParent.offsetTop

  document.addEventListener('scroll', function(e) {
    window.scrollY >= offsetTop ? sidebar.classList.add('sticky') :
                                  sidebar.classList.remove('sticky');
  });

  toc.addEventListener('click', function(e) {
    var parent = e.target.parentElement;
    if (e.target.localName == 'a' && parent.classList.contains('toplevel')) {
      // Allow normal link click if  h2 toplevel heading doesn't have h3s.
      if (parent.querySelector('.toc')) {
        e.preventDefault();
        parent.classList.toggle('active');
      }
    }
  });

} else {
  // Prevent permalinks from appearong on mobile.
  document.body.classList.add('no-permalink');

  articleBody.addEventListener('click', function(e) {
    if (e.target.localName == 'h2') {
      e.target.parentElement.classList.toggle('active');
    }
  });
}

addPermalinkHeadings(articleBody);

}());
