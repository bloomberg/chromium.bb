// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function adbPages() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'adb-pages', false);
  xhr.send(null);
  if (xhr.status !== 200)
    return null;

  try {
    return JSON.parse(xhr.responseText);
  } catch (e) {
  }
  return null;
}

function requestData() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'targets-data.json', false);
  xhr.send(null);
  if (xhr.status === 200)
    return JSON.parse(xhr.responseText);
  return [];
}

function inspect(data) {
  chrome.send('inspect', [data]);
}

function terminate(data) {
  chrome.send('terminate', [data]);
}

function removeChildren(element_id) {
  var element = document.getElementById(element_id);
  element.textContent = '';
}

function onload() {
  populateLists();
  populateDeviceLists();
}

function populateLists() {
  var data = requestData();

  removeChildren('pages');
  removeChildren('extensions');
  removeChildren('workers');
  removeChildren('others');

  for (var i = 0; i < data.length; i++) {
    if (data[i].type === 'page')
      addToPagesList(data[i]);
    else if (data[i].type === 'worker')
      addToWorkersList(data[i]);
    else if (data[i].type === 'extension')
      addToExtensionsList(data[i]);
    else
      addToOthersList(data[i]);
  }
}

function populateDeviceLists() {
  var pages = adbPages();
  var pagesDigest = JSON.stringify(pages);
  if (!pages || pagesDigest == window.pagesDigest) {
    setTimeout(populateDeviceLists, 1000);
    return;
  }
  window.pagesDigest = pagesDigest;

  // Clear existing entries
  var deviceElements = document.querySelectorAll('.device');
  for (var i = 0; i < deviceElements.length; i++)
    deviceElements[i].remove();

  // Populate with new entries
  for (var i = 0; pages && i < pages.length; i++) {
    var page = pages[i];

    var listId = 'device-' + page.adbSerial;
    var listElement = document.getElementById(listId);
    if (!listElement) {
      var sectionElement = document.createElement('div');
      sectionElement.className = 'section device';
      sectionElement.textContent = page.adbModel;

      listElement = document.createElement('div');
      listElement.className = 'list device';
      listElement.id = listId;
      document.body.appendChild(sectionElement);
      document.body.appendChild(listElement);
    }

    addTargetToList(page, listId, ['faviconUrl', 'name', 'url']);
  }

  setTimeout(populateDeviceLists, 1000);
}

function addToPagesList(data) {
  addTargetToList(data, 'pages', ['faviconUrl', 'name', 'url']);
}

function addToExtensionsList(data) {
  addTargetToList(data, 'extensions', ['name', 'url']);
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
  var list = document.getElementById(listId);
  var row = document.createElement('div');
  row.className = 'row';
  for (var j = 0; j < properties.length; j++)
    row.appendChild(formatValue(data, properties[j]));

  if (!data['adbSerial'] || data['adbDebugUrl'])
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
