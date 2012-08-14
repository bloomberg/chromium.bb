// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  /**
   * Toggles the display of nodes given the status of their associated controls.
   *
   * For each node passed to this function, check to see if a toggle has been
   * inserted into the node's parent.  If yes, change the state of the toggle
   * and hide/reveal the node as needed.
   *
   * @param {NodeList|Node|Array.<Node>} Nodes to operate on.
   */
  function toggleList(list) {
    if (typeof list.length != 'number') {
      list = Array(list);
    }

    for (var i = 0; i < list.length; i++) {
      var toggle = list[i].parentNode &&
                   list[i].parentNode.firstChild;
      if (toggle && toggle.className.substring(0, 6) == 'toggle') {
        var visible = toggle.className == 'toggle';
        list[i].style.display = visible ? 'block' : 'none';
        toggle.className = visible ? 'toggle selected' : 'toggle';
      }
    }
  };

  /**
   * Reveals the hidden ancestor of the passed node, adjusts toggles as needed.
   *
   * @param {Node} node The node whose ancestor is a hidden toggleable element.
   */
  function revealAncestor(node) {
    while (node.parentNode) {
      if (node.style.display == 'none') {
        toggleList(node);
        break;
      }
      node = node.parentNode;
    }
  };

  /**
   * Adds toggle controls to the sidebar list.
   *
   * Controls are inserted as the first children of list items in the sidebar
   * which contain only text (not a link).  Handlers are set up so that when a
   * toggle control is clicked, any <ul> elements who are siblings of the
   * control are hidden/revealed as appropriate given the control's state.
   *
   * If a list item possesses the class "leftNavSelected" its ancestor <ul> is
   * revealed by default (it represents the current page).
   */
  function initSidebar() {
    var toc = document.getElementById('gc-toc');
    if (!toc)
      return;

    // Figure out which link matches the current page so it can be styled
    // differently.
    var links = toc.getElementsByTagName('a');
    var selectedNode = null;
    var path_parts = document.location.pathname.replace(/\/+$/, '').split('/')
    for (var i = 0; i < links.length; i++) {
      if (links[i].getAttribute('href') != path_parts[path_parts.length - 1])
        continue;
      links[i].className = 'leftNavSelected';
      links[i].removeAttribute('href');
      selectedNode = links[i];
    }

    // Go through the items on the sidebar and add toggles.
    var items = toc.getElementsByTagName('li');
    for (var i = 0; i < items.length; i++) {
      var item = items[i];
      if (!item.firstChild || item.firstChild.tagName != 'SPAN')
        continue;
      // Only assign toggles to text nodes in the sidebar.
      var a = document.createElement('a');
      a.className = 'toggle selected';
      a.appendChild(document.createTextNode(' '));
      a.onclick = function() {
        toggleList(this.parentNode.getElementsByTagName('ul'));
      };
      item.firstChild.onclick = function() {
        toggleList(this.parentNode.getElementsByTagName('ul'));
      };
      item.insertBefore(a, item.firstChild);
      toggleList(item.getElementsByTagName('ul'));
    }

    // Reveal the selected link.
    if (selectedNode)
      revealAncestor(selectedNode);
  };

  initSidebar();
})()
