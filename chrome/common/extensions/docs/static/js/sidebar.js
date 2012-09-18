// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Adds toggle controls to the sidebar list.
 *
 * Controls are inserted as the first children of list items in the sidebar
 * which contain only text (not a link).  Handlers are set up so that when a
 * toggle control is clicked, any <ul> elements who are siblings of the
 * control are hidden/revealed as appropriate given the control's state.
 *
 * If a list item possesses the class 'selected' its ancestor <ul> is
 * revealed by default (it represents the current page).
 */
(function() {
  var sidebar = document.getElementById('gc-sidebar');
  if (!sidebar)
    return;

  // Figure out which link matches the current page so it can be styled
  // differently.
  var pathParts = document.location.pathname.replace(/\/+$/, '').split('/')
  Array.prototype.forEach.call(sidebar.getElementsByTagName('a'),
                               function(link) {
    if (link.getAttribute('href') != pathParts[pathParts.length - 1])
      return;
    link.className = 'selected';
    link.removeAttribute('href');
  });

  // Go through the items on the sidebar and add toggles.
  Array.prototype.forEach.call(sidebar.querySelectorAll('[toggleable]'),
                               function(toggleable) {
    var buttonContent = toggleable.previousElementSibling;
    if (!buttonContent) {
      console.warn('Cannot toggle %s, no previous sibling', toggleable);
      return;
    }

    var button = document.createElement('a');
    button.className = 'button';
    var toggleIndicator = document.createElement('div');
    toggleIndicator.className = 'toggleIndicator';

    var isToggled = false;
    function toggle() {
      if (isToggled) {
        toggleable.classList.add('hidden');
        toggleIndicator.classList.remove('toggled');
      } else {
        toggleable.classList.remove('hidden');
        toggleIndicator.classList.add('toggled');
      }
      isToggled = !isToggled;
    }

    // TODO(kalman): needs a button role for accessibility?
    button.setAttribute('href', 'javascript:void(0)');
    button.addEventListener('click', toggle);
    buttonContent.parentElement.insertBefore(button, buttonContent);
    button.appendChild(buttonContent);
    button.appendChild(toggleIndicator);

    // Leave the toggle open if the selected link is a child.
    if (toggleable.querySelector('.selected'))
      toggle();
    else
      toggleable.classList.add('hidden');
  });

  // Each level of the sidebar is displayed differently. Rather than trying
  // to nest CSS selectors in a crazy way, programmatically assign levels
  // to them.
  function addNestLevels(node, currentLevel) {
    node.classList.add('level' + currentLevel);
    if (node.tagName == 'UL')
      currentLevel++;
    Array.prototype.forEach.call(node.children, function(child) {
      addNestLevels(child, currentLevel);
    });
  }

  addNestLevels(sidebar, 1);
})()
