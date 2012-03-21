// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function requestData() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'targets-data.json', false);
  xhr.send(null);
  if (xhr.status === 200)
    return JSON.parse(xhr.responseText);
  return [];
}

function inspect(data) {
  chrome.send('inspect',
              [String(data.processId), String(data.routeId)]);
}

function terminate(data) {
  chrome.send('terminate',
              [String(data.processId), String(data.routeId)]);
}

function removeChildren(element_id) {
  var element = document.getElementById(element_id);
  element.textContent = '';
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

function addToPagesList(data) {
  addTargetToList(data, 'pages', ['favicon_url', 'name', 'url']);
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

  if (property == 'favicon_url') {
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

document.addEventListener('DOMContentLoaded', populateLists);
