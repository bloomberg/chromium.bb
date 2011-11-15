// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hungRendererDialog', function() {
  'use strict';

  /**
   * Disables the controls while the dialog is busy.
   */
  function disableControls() {
    $('kill').disabled = true;
    $('wait').disabled = true;
  }

  /**
   * Close the dialog and pass a result value to the dialog close handler.
   * @param {boolean} result The value to pass to the dialog close handler.
   */
  function closeWithResult(result) {
    disableControls();
    var json = JSON.stringify([result]);
    chrome.send('DialogClose', [json]);
  }

  /**
   * Reverses the order of child nodes. This is necessary because WebKit does
   * not alter the tab order for elements that are visually reversed using
   * -webkit-box-direction: reverse, and the button order is reversed for
   * views.  See https://bugs.webkit.org/show_bug.cgi?id=62664 for more
   * information.
   * @param {HTMLElement} parent The parent node whose children are to be
   *     reversed.
   */
  function reverseChildren(parent) {
    var childNodes = parent.childNodes;
    for (var i = childNodes.length - 1; i >= 0; i--)
      parent.appendChild(childNodes[i]);
  };

  var forEach = Array.prototype.forEach.call.bind(Array.prototype.forEach);

  /**
   * Inserts translated strings on loading.
   */
  function initialize() {
    i18nTemplate.process(document, templateData);

    $('kill').onclick = function() {
      closeWithResult(true);
    }

    $('wait').onclick = function() {
      closeWithResult(false);
    }

    if (cr.isViews)
      forEach(document.querySelectorAll('.button-strip'), reverseChildren);
    chrome.send('requestTabContentsList');
  }

  /**
   * Adds elements to the DOM to populate the tab contents list box area of the
   * dialog. Substitutes the favicon source and title text from the details
   * using a template mechanism (clones hidden parts of the dialog DOM).
   * @param {Array.<{title: string, url: string}>} tabDetailsList Array of tab
   *     contents detail objects containing |url| and |title| for each frozen
   *     tab.
   */
  function setTabContentsList(tabDetailsList) {
    var numTabs = tabDetailsList.length;
    for (var i = 0; i < numTabs; i++) {
      var listItem = document.createElement('li');
      listItem.style.backgroundImage = url('chrome://favicon/size/32/' +
                                           tabDetailsList[i].url);
      listItem.textContent = tabDetailsList[i].title;
      $('tab-table').appendChild(listItem);
    }
  }

  return {
    initialize: initialize,
    setTabContentsList: setTabContentsList,
  };
});

document.addEventListener('DOMContentLoaded', hungRendererDialog.initialize);
