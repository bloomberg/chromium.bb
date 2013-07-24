// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function inspect(data) {
  chrome.send('inspect', [data]);
}

function terminate(data) {
  chrome.send('terminate', [data]);
}

function removeChildren(element_id) {
  var element = $(element_id);
  element.textContent = '';
}

function onload() {
  var tabContents = document.querySelectorAll('#content > div');
  for (var i = 0; i != tabContents.length; i++) {
    var tabContent = tabContents[i];
    var tabName = tabContent.querySelector('.content-header').textContent;

    var tabHeader = document.createElement('div');
    tabHeader.className = 'tab-header';
    var button = document.createElement('button');
    button.textContent = tabName;
    tabHeader.appendChild(button);
    tabHeader.addEventListener('click', selectTab.bind(null, tabContent.id));
    $('navigation').appendChild(tabHeader);
  }
  selectTab('devices-tab');
  chrome.send('init-ui');
}

function selectTab(id) {
  var tabContents = document.querySelectorAll('#content > div');
  var tabHeaders = $('navigation').querySelectorAll('.tab-header');
  for (var i = 0; i != tabContents.length; i++) {
    var tabContent = tabContents[i];
    var tabHeader = tabHeaders[i];
    if (tabContent.id == id) {
      tabContent.classList.add('selected');
      tabHeader.classList.add('selected');
    } else {
      tabContent.classList.remove('selected');
      tabHeader.classList.remove('selected');
    }
  }
}

function populateLists(data) {
  removeChildren('pages');
  removeChildren('extensions');
  removeChildren('apps');
  removeChildren('workers');
  removeChildren('others');

  for (var i = 0; i < data.length; i++) {
    if (data[i].type === 'page')
      addToPagesList(data[i]);
    else if (data[i].type === 'worker')
      addToWorkersList(data[i]);
    else if (data[i].type === 'extension')
      addToExtensionsList(data[i]);
    else if (data[i].type === 'app')
      addToAppsList(data[i]);
    else
      addToOthersList(data[i]);
  }
}

function populateDeviceLists(pages) {
  var pagesDigest = JSON.stringify(pages);
  if (!pages || pagesDigest == window.pagesDigest)
    return;

  window.pagesDigest = pagesDigest;

  // Clear existing entries
  var deviceElements = document.querySelectorAll('.device');
  for (var i = 0; i < deviceElements.length; i++)
    deviceElements[i].remove();
  var containerElement = $('devices');

  // Populate with new entries
  for (var i = 0; pages && i < pages.length; i++) {
    var page = pages[i];

    var listId = 'device-' + page.adbSerial;
    var listElement = $(listId);
    if (!listElement) {
      var sectionElement = document.createElement('div');
      sectionElement.className = 'section device';
      sectionElement.textContent = page.adbModel;

      listElement = document.createElement('div');
      listElement.className = 'list device';
      listElement.id = listId;
      containerElement.appendChild(sectionElement);
      containerElement.appendChild(listElement);
    }

    var packageId = 'package-' + page.adbModel + '-' + page.adbPackage;
    var packageElement = $(packageId);
    if (!packageElement) {
      var sectionElement = document.createElement('div');
      sectionElement.className = 'small-section package';
      sectionElement.textContent = page.adbPackage;

      packageElement = document.createElement('div');
      packageElement.className = 'list package';
      packageElement.id = packageId;
      listElement.appendChild(sectionElement);
      listElement.appendChild(packageElement);
    }

    addTargetToList(page, packageId, ['faviconUrl', 'name', 'url']);
  }
}

function addToPagesList(data) {
  addTargetToList(data, 'pages', ['faviconUrl', 'name', 'url']);
}

function addToExtensionsList(data) {
  addTargetToList(data, 'extensions', ['name', 'url']);
}

function addToAppsList(data) {
  addTargetToList(data, 'apps', ['name', 'url']);
}

function addToWorkersList(data) {
  addTargetToList(data,
                  'workers',
                  ['name', 'url', 'pid'],
                  true);
}

function addToOthersList(data) {
  addTargetToList(data, 'others', ['url']);
}

function formatValue(data, property) {
  var value = data[property];

  if (property == 'faviconUrl') {
    var faviconElement = document.createElement('img');
    if (value)
      faviconElement.src = value;
    return faviconElement;
  }

  var text = value ? String(value) : '';
  if (text.length > 100)
    text = text.substring(0, 100) + '\u2026';

  if (property == 'pid')
    text = 'Pid:' + text;

  var span = document.createElement('span');
  span.textContent = ' ' + text + ' ';
  span.className = property;
  return span;
}

function addTargetToList(data, listId, properties, canTerminate) {
  var list = $(listId);
  var row = document.createElement('div');
  row.className = 'row';
  for (var j = 0; j < properties.length; j++)
    row.appendChild(formatValue(data, properties[j]));

  if (!data.hasOwnProperty('adbSerial') || data['adbDebugUrl'])
    row.appendChild(createInspectElement(data));

  if (canTerminate)
    row.appendChild(createTerminateElement(data));

  row.processId = data.processId;
  row.routeId = data.routeId;

  list.appendChild(row);
}

function createInspectElement(data) {
  var link = document.createElement('a');
  link.setAttribute('href', '#');
  link.textContent = ' inspect ';
  link.addEventListener(
      'click',
      inspect.bind(this, data),
      true);
  return link;
}

function createTerminateElement(data) {
  var link = document.createElement('a');
  if (data.attached)
    link.disabled = true;

  link.setAttribute('href', '#');
  link.textContent = ' terminate ';
  link.addEventListener(
      'click',
      terminate.bind(this, data),
      true);
  return link;
}

document.addEventListener('DOMContentLoaded', onload);
