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

  function toggleAccessibility(data, element, mode) {
    chrome.send(
        'toggleAccessibility',
        [String(data.processId), String(data.routeId), mode]);
    document.location.reload();
  }

  function requestWebContentsTree(data, element) {
    chrome.send(
        'requestWebContentsTree',
        [String(data.processId), String(data.routeId)]);
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

    var list = data['list'];
    for (var i = 0; i < list.length; i++) {
      addToPagesList(list[i]);
    }

    var showNativeUI = $('showNativeUI');
    showNativeUI.addEventListener('click', function() {
      var delay = $('native_ui_delay').value;
      setTimeout(function() {
        chrome.send('requestNativeUITree');
      }, delay);
    });
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
    var id = data['processId'] + '.' + data['routeId'];
    var row = document.createElement('div');
    row.className = 'row';
    row.id = id;
    formatRow(row, data);

    row.processId = data.processId;
    row.routeId = data.routeId;

    var list = $('pages');
    list.appendChild(row);
  }

  function formatRow(row, data) {
    if (!('url' in data)) {
      if ('error' in data) {
        row.appendChild(createErrorMessageElement(data, row));
        return;
      }
    }

    var siteInfo = document.createElement('div');
    var properties = ['favicon_url', 'name', 'url'];
    for (var j = 0; j < properties.length; j++)
      siteInfo.appendChild(formatValue(data, properties[j]));
    row.appendChild(siteInfo);

    row.appendChild(createModeElement(AXMode.kNativeAPIs, data));
    row.appendChild(createModeElement(AXMode.kWebContents, data));
    row.appendChild(createModeElement(AXMode.kInlineTextBoxes, data));
    row.appendChild(createModeElement(AXMode.kScreenReader, data));
    row.appendChild(createModeElement(AXMode.kHTML, data));

    row.appendChild(document.createTextNode(' | '));

    if ('tree' in data) {
      row.appendChild(createShowAccessibilityTreeElement(data, row, true));
      if (navigator.clipboard) {
        row.appendChild(createCopyAccessibilityTreeElement(row.id));
      }
      row.appendChild(createHideAccessibilityTreeElement(row.id));
      row.appendChild(createAccessibilityTreeElement(data));
    } else {
      row.appendChild(createShowAccessibilityTreeElement(data, row, false));
      if ('error' in data)
        row.appendChild(createErrorMessageElement(data, row));
    }
  }

  function insertHeadingInline(parentElement, headingText) {
    var h4 = document.createElement('h4');
    h4.textContent = headingText;
    h4.style.display = 'inline';
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
      insertHeadingInline(span, content);
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

  function createShowAccessibilityTreeElement(data, row, opt_refresh) {
    let show = document.createElement('button');
    if (opt_refresh) {
      show.textContent = 'Refresh accessibility tree';
    } else {
      show.textContent = 'Show accessibility tree';
    }
    show.id = row.id + ':showTree';
    show.addEventListener(
        'click', requestWebContentsTree.bind(this, data, show));
    return show;
  }

  function createHideAccessibilityTreeElement(id) {
    let hide = document.createElement('button');
    hide.textContent = 'Hide accessibility tree';
    hide.addEventListener('click', function() {
      $(id + ':showTree').textContent = 'Show accessibility tree';
      var existingTreeElements = $(id).getElementsByTagName('pre');
      for (var i = 0; i < existingTreeElements.length; i++) {
        $(id).removeChild(existingTreeElements[i]);
      }
      var row = $(id);
      while (row.lastChild != $(id + ':showTree')) {
        row.removeChild(row.lastChild);
      }
    });
    return hide;
  }

  function createCopyAccessibilityTreeElement(id) {
    let copy = document.createElement('button');
    copy.textContent = 'Copy accessibility tree';
    copy.addEventListener('click', () => {
      // |id| refers to the div containing accessibility information for a
      // single page, so there should only be one <pre> child containing the
      // accessibility tree as a string.
      let tree = $(id).getElementsByTagName('pre')[0];
      navigator.clipboard.writeText(tree.textContent)
          .then(() => {
            copy.textContent = 'Copied to clipboard!';
            setTimeout(() => {
              copy.textContent = 'Copy accessibility tree';
            }, 5000);
          })
          .catch(err => {
            console.err('Unable to copy accessibility tree.', err);
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
    var id = data.processId + '.' + data.routeId;
    var row = $(id);
    if (!row)
      return;

    row.textContent = '';
    formatRow(row, data);
  }

  // Called from C++
  function showNativeUITree(data) {
    var treeContainer = document.querySelector('#native_ui div');
    if (!treeContainer) {
      var treeContainer = document.createElement('div');
      $('native_ui').appendChild(treeContainer);
    }

    var dstIds =
        new Set(Array.prototype.map.call(treeContainer.children, el => el.id));
    data.forEach(function(browser) {
      var srcId = 'browser_' + browser.id;
      if (dstIds.has(srcId)) {
        // Update browser windows in place.
        dstIds.delete(srcId);
        var title = document.querySelector('#' + srcId + ' h4');
        title.textContent = browser.title;
        var tree = document.querySelector('#' + srcId + ' pre');
        tree.textContent = browser.tree;
      } else {
        // Add new browser windows.
        var browserElement = createNativeUITreeElement(browser);
        treeContainer.appendChild(browserElement);
      }
    });
    dstIds.forEach(function(dstId) {
      // Remove browser windows that no longer exist.
      var browserElement = document.querySelector('#' + dstId);
      treeContainer.removeChild(browserElement);
    });
  }

  function createNativeUITreeElement(browser) {
    var details = document.createElement('details');
    var summary = document.createElement('summary');
    var treeElement = document.createElement('pre');
    insertHeadingInline(summary, browser.title);
    treeElement.textContent = browser.tree;
    details.id = 'browser_' + browser.id;
    details.appendChild(summary);
    details.appendChild(treeElement);
    return details;
  }

  function createAccessibilityTreeElement(data) {
    var treeElement = document.createElement('pre');
    var tree = data.tree;
    treeElement.textContent = tree;
    return treeElement;
  }

  // These are the functions we export so they can be called from C++.
  return {
    initialize: initialize,
    showTree: showTree,
    showNativeUITree: showNativeUITree
  };
});

document.addEventListener('DOMContentLoaded', accessibility.initialize);
