// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('accessibility', function() {
  'use strict';

  // Note: keep these values in sync with the values in
  // content/common/accessibility_mode_enums.h
  const AXMode = {
    kNativeAPIs: 1 << 0,
    kWebContents: 1 << 1,
    kInlineTextBoxes: 1 << 2,
    kScreenReader: 1 << 3,
    kHTML: 1 << 4,

    get kAXModeWebContentsOnly() {
      return AXMode.kWebContents | AXMode.kInlineTextBoxes |
          AXMode.kScreenReader | AXMode.kHTML;
    },

    get kAXModeComplete() {
      return AXMode.kNativeAPIs | AXMode.kWebContents |
          AXMode.kInlineTextBoxes | AXMode.kScreenReader | AXMode.kHTML;
    }
  };

  function requestData() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', 'targets-data.json', false);
    xhr.send(null);
    if (xhr.status === 200) {
      console.log(xhr.responseText);
      return JSON.parse(xhr.responseText);
    }
    return [];
  }

  function getIdFromData(data) {
    if (data.type == 'page') {
      return data.processId + '.' + data.routeId;
    } else if (data.type == 'browser') {
      return 'browser.' + data.sessionId;
    } else {
      console.error('Unknown data type.', data);
      return '';
    }
  }

  function toggleAccessibility(data, element, mode) {
    chrome.send(
        'toggleAccessibility',
        [String(data.processId), String(data.routeId), mode]);
    document.location.reload();
  }

  function requestTree(data, element) {
    if (data.type == 'browser') {
      let delay = $('native_ui_delay').value;
      setTimeout(() => {
        chrome.send('requestNativeUITree', [String(data.sessionId)]);
      }, delay);
    } else {
      chrome.send(
          'requestWebContentsTree',
          [String(data.processId), String(data.routeId)]);
    }
  }

  function initialize() {
    console.log('initialize');
    var data = requestData();

    bindCheckbox('native', data['native']);
    bindCheckbox('web', data['web']);
    bindCheckbox('text', data['text']);
    bindCheckbox('screenreader', data['screenreader']);
    bindCheckbox('html', data['html']);
    bindCheckbox('internal', data['internal']);

    $('pages').textContent = '';

    let pages = data['pages'];
    for (var i = 0; i < pages.length; i++) {
      addToPagesList(pages[i]);
    }

    let browsers = data['browsers'];
    for (let i = 0; i < browsers.length; i++) {
      addToBrowsersList(browsers[i]);
    }
  }

  function bindCheckbox(name, value) {
    if (value == 'on')
      $(name).checked = true;
    if (value == 'disabled') {
      $(name).disabled = true;
      $(name).labels[0].classList.add('disabled');
    }
    $(name).addEventListener('change', function() {
      chrome.send('setGlobalFlag', [name, $(name).checked]);
      document.location.reload();
    });
  }

  function addToPagesList(data) {
    // TODO: iterate through data and pages rows instead
    let id = getIdFromData(data);
    let row = document.createElement('div');
    row.className = 'row';
    row.id = id;
    formatRow(row, data);

    row.processId = data.processId;
    row.routeId = data.routeId;

    let pages = $('pages');
    pages.appendChild(row);
  }

  function addToBrowsersList(data) {
    let id = getIdFromData(data);
    let row = document.createElement('div');
    row.className = 'row';
    row.id = id;
    formatRow(row, data);

    let browsers = $('browsers');
    browsers.appendChild(row);
  }

  function formatRow(row, data) {
    if (!('url' in data)) {
      if ('error' in data) {
        row.appendChild(createErrorMessageElement(data, row));
        return;
      }
    }

    if (data.type == 'page') {
      let siteInfo = document.createElement('div');
      let properties = ['favicon_url', 'name', 'url'];
      for (var j = 0; j < properties.length; j++)
        siteInfo.appendChild(formatValue(data, properties[j]));
      row.appendChild(siteInfo);

      row.appendChild(createModeElement(AXMode.kNativeAPIs, data));
      row.appendChild(createModeElement(AXMode.kWebContents, data));
      row.appendChild(createModeElement(AXMode.kInlineTextBoxes, data));
      row.appendChild(createModeElement(AXMode.kScreenReader, data));
      row.appendChild(createModeElement(AXMode.kHTML, data));
    } else {
      let siteInfo = document.createElement('span');
      siteInfo.appendChild(formatValue(data, 'name'));
      row.appendChild(siteInfo);
    }

    row.appendChild(document.createTextNode(' | '));

    if ('tree' in data) {
      row.appendChild(createTreeButtons(data, row.id));
    } else {
      row.appendChild(createShowAccessibilityTreeElement(data, row.id, false));
      if ('error' in data)
        row.appendChild(createErrorMessageElement(data, row));
    }
  }

  function insertHeadingInline(parentElement, headingText, id) {
    var h4 = document.createElement('h4');
    h4.textContent = headingText;
    h4.style.display = 'inline';
    h4.id = id + ':title';
    parentElement.appendChild(h4);
  }

  function formatValue(data, property) {
    var value = data[property];

    if (property == 'favicon_url') {
      var faviconElement = document.createElement('img');
      if (value)
        faviconElement.src = value;
      faviconElement.alt = '';
      return faviconElement;
    }

    var text = value ? String(value) : '';
    if (text.length > 100)
      text = text.substring(0, 100) + '\u2026';  // ellipsis

    var span = document.createElement('span');
    var content = ' ' + text + ' ';
    if (property == 'name') {
      let id = getIdFromData(data);
      insertHeadingInline(span, content, id);
    } else {
      span.textContent = content;
    }
    span.className = property;
    return span;
  }

  function getNameForAccessibilityMode(mode) {
    switch (mode) {
      case AXMode.kNativeAPIs:
        return 'Native';
      case AXMode.kWebContents:
        return 'Web';
      case AXMode.kInlineTextBoxes:
        return 'Inline text';
      case AXMode.kScreenReader:
        return 'Screen reader';
      case AXMode.kHTML:
        return 'HTML';
    }
    return 'unknown';
  }

  function createModeElement(mode, data) {
    var currentMode = data['a11y_mode'];
    var link = document.createElement('a', 'action-link');
    link.setAttribute('role', 'button');

    var stateText = ((currentMode & mode) != 0) ? 'true' : 'false';
    link.textContent = getNameForAccessibilityMode(mode) + ': ' + stateText;
    link.setAttribute('aria-pressed', stateText);
    link.addEventListener(
        'click', toggleAccessibility.bind(this, data, link, mode));
    return link;
  }

  function createTreeButtons(data, id) {
    let row = document.createElement('span');
    row.appendChild(createShowAccessibilityTreeElement(data, id, true));
    if (navigator.clipboard) {
      row.appendChild(createCopyAccessibilityTreeElement(id));
    }
    row.appendChild(createHideAccessibilityTreeElement(id));
    row.appendChild(createAccessibilityTreeElement(data, id));
    return row;
  }

  function createShowAccessibilityTreeElement(data, id, opt_refresh) {
    let show = document.createElement('button');
    if (opt_refresh) {
      show.textContent = 'Refresh accessibility tree';
    } else {
      show.textContent = 'Show accessibility tree';
    }
    show.id = id + ':showTree';
    show.addEventListener('click', requestTree.bind(this, data, show));
    return show;
  }

  function createHideAccessibilityTreeElement(id) {
    let hide = document.createElement('button');
    hide.textContent = 'Hide accessibility tree';
    hide.id = id + ':hideTree';
    hide.addEventListener('click', function() {
      $(id + ':showTree').textContent = 'Show accessibility tree';
      let elements = ['hideTree', 'tree'];
      for (let i = 0; i < elements.length; i++) {
        let elt = $(id + ':' + elements[i]);
        if (elt) {
          elt.style.display = 'none';
        }
      }
    });
    return hide;
  }

  function createCopyAccessibilityTreeElement(id) {
    let copy = document.createElement('button');
    copy.textContent = 'Copy accessibility tree';
    copy.id = id + ':copyTree';
    copy.addEventListener('click', () => {
      let tree = $(id + ':tree');
      navigator.clipboard.writeText(tree.textContent)
          .then(() => {
            copy.textContent = 'Copied to clipboard!';
            setTimeout(() => {
              copy.textContent = 'Copy accessibility tree';
            }, 5000);
          })
          .catch(err => {
            console.error('Unable to copy accessibility tree.', err);
          });
    });
    return copy;
  }

  function createErrorMessageElement(data) {
    var errorMessageElement = document.createElement('div');
    var errorMessage = data.error;
    errorMessageElement.innerHTML = errorMessage + '&nbsp;';
    var closeLink = document.createElement('a');
    closeLink.href = '#';
    closeLink.textContent = '[close]';
    closeLink.addEventListener('click', function() {
      var parentElement = errorMessageElement.parentElement;
      parentElement.removeChild(errorMessageElement);
      if (parentElement.childElementCount == 0)
        parentElement.parentElement.removeChild(parentElement);
    });
    errorMessageElement.appendChild(closeLink);
    return errorMessageElement;
  }

  // Called from C++
  function showTree(data) {
    let id = getIdFromData(data);
    let row = $(id);
    if (!row)
      return;

    row.textContent = '';
    formatRow(row, data);
  }

  function createNativeUITreeElement(browser) {
    let id = 'browser.' + browser.id;
    let row = document.createElement('div');
    row.className = 'row';
    row.id = id;
    formatRow(row, browser);
    return row;
  }

  function createAccessibilityTreeElement(data, id) {
    var treeElement = $(id + ':tree');
    if (treeElement) {
      treeElement.style.display = '';
    } else {
      treeElement = document.createElement('pre');
      treeElement.id = id + ':tree';
    }
    treeElement.textContent = data.tree;
    return treeElement;
  }

  // These are the functions we export so they can be called from C++.
  return {initialize: initialize, showTree: showTree};
});

document.addEventListener('DOMContentLoaded', accessibility.initialize);
