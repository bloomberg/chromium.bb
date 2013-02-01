// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('media', function() {
  'use strict';

  /**
   * The width and height of a bar drawn on a file canvas in pixels.
   */
  var BAR_WIDTH = 500;
  var BAR_HEIGHT = 16;

  /**
   * Draws a 1px white horizontal line across |context|.
   */
  function drawLine(context, top) {
    context.moveTo(0, top);
    context.lineTo(BAR_WIDTH, top);
    context.strokeStyle = '#fff';
    context.stroke();
  }

  /**
   * Creates an HTMLElement of type |type| with textContent |content|.
   * @param {string} type The type of element to create.
   * @param {string} content The content to place in the element.
   * @return {HTMLElement} A newly initialized element.
   */
  function makeElement(type, content) {
    var element = document.createElement(type);
    element.textContent = content;
    return element;
  }

  /**
   * Creates a new <li> containing a <details> with a <summary> and sets
   * properties to reference them.
   * @return {Object} The new <li>.
   */
  function createDetailsLi() {
    var li = document.createElement('li');
    li.details = document.createElement('details');
    li.summary = document.createElement('summary');
    li.appendChild(li.details);
    li.details.appendChild(li.summary);
    return li
  }

  /**
   * Appends each key-value pair in a dictionary to a row in a table.
   * @param {Object} dict The dictionary to append.
   * @param {HTMLElement} table The <table> element to append to.
   */
  function appendDictionaryToTable(dict, table) {
    table.textContent = '';
    for (var key in dict) {
      var tr = document.createElement('tr');
      tr.appendChild(makeElement('td', key + ':'));
      tr.appendChild(makeElement('td', dict[key]));
      table.appendChild(tr);
    }
    return table;
  }

  return {
    BAR_WIDTH: BAR_WIDTH,
    BAR_HEIGHT: BAR_HEIGHT,
    drawLine: drawLine,
    makeElement: makeElement,
    createDetailsLi: createDetailsLi,
    appendDictionaryToTable: appendDictionaryToTable
  };
});
