// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * ADB Device representation. This class has static methods for querying for
 * devices as well as instance methods for device manipulation.
 * @param {string} deviceLine Raw device descriprion line.
 * @constructor
 */
function AdbDevice(deviceLine) {
  var tokens = deviceLine.split(/[ \t]/);
  this.serial = tokens[0];
  var modelQuery = 'host:transport:' + this.serial +
      '|shell:getprop ro.product.model';
  this.model = String(AdbDevice.adbQuery_(modelQuery)).trim();
}

/**
 * Returns discoverable devices, establishes port forwarding for them.
 * @return {Array.<AdbDevice>} Device array.
 */
AdbDevice.queryDevices = function() {
  var deviceList = AdbDevice.adbQuery_('host:devices');
  if (!deviceList)
    return [];

  var forwards = AdbDevice.collectForwards_();

  var rows = deviceList.split('\n');
  var devices = [];
  for (var i = 0; i < rows.length; i++) {
    if (!rows[i])
      continue;
    var device = new AdbDevice(rows[i]);
    devices.push(device);

    // Assign / bind TCP ports.
    device.tcpPort = forwards[device.serial];
    if (!device.tcpPort) {
      var port = AdbDevice.nextAvailablePort_(forwards);
      if (!port)
        continue;
      AdbDevice.adbQuery_('host-serial:' + device.serial + ':forward:tcp:' +
          port + ';localabstract:chrome_devtools_remote');
      forwards = AdbDevice.collectForwards_();
    }
  }

  return devices;
};

/**
 * Collects and returns port forward map for all connected devices.
 * @return {Array.<Object<string, string>>} Forwarding map.
 * @private
 */
AdbDevice.collectForwards_ = function() {
  var response = AdbDevice.adbQuery_('host:list-forward');
  if (!response)
    return [];

  var forwards = {};
  var rows = response.split('\n');
  for (var i = 0; i < rows.length; i++) {
    var row = rows[i];
    if (!row)
      continue;

    var tokens = row.split(' ');
    if (tokens.length != 3 ||
        tokens[1].indexOf('tcp:') != 0 ||
        tokens[2] != 'localabstract:chrome_devtools_remote')
      continue;
    var tcpPort = tokens[1].substring(4);
    forwards[tokens[0]] = tcpPort;
  }
  return forwards;
};

/**
 * Issues synchronous adb query.
 * @param {string} query ADB query.
 * @return {?Object} ADB query result.
 * @private
 */
AdbDevice.adbQuery_ = function(query) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'adb-query/' + query, false);
  xhr.send(null);
  if (xhr.status !== 200)
    return null;

  try {
    var result = JSON.parse(xhr.responseText);
    return result[0] ? null : result[1];
  } catch (e) {
  }
  return null;
};

/**
 * Discovers ADB devices.
 * @return {?Object} ADB query result.
 * @private
 */
AdbDevice.adbDevices_ = function() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'adb-devices', false);
  xhr.send(null);
  if (xhr.status !== 200)
    return null;

  try {
    var result = JSON.parse(xhr.responseText);
    return result[0] ? null : result[1];
  } catch (e) {
  }
  return null;
};

/**
 * Returns next available (unmapped) port to use in forwarding.
 * @param {Object<string,string>} forwards Forwards map.
 * @return {number} Next available port.
 * @private
 */
AdbDevice.nextAvailablePort_ = function(forwards) {
  for (var port = 9232; port < 9252; port++) {
    var taken = false;
    for (var serial in forwards) {
       if (forwards[serial] == port) {
         taken = true;
         break;
       }
    }
    if (taken)
      continue;
    return port;
  }
  return 0;
};

/**
 * Returns /json/version JSON object with target device description.
 * @return {?Object} Version object.
 */
AdbDevice.prototype.version = function() {
  return this.queryJson_('version');
};

/**
 * Returns the list of inspectable targets in the target format suitable
 * for rendering as target rows.
 * @return {Array.<Object>} Target list.
 */
AdbDevice.prototype.targets = function() {
  var pages = this.queryJson_('list');
  var targets = [];
  for (var j = 0; pages && j < pages.length; j++) {
    var json = pages[j];
    var target = {};
    target['type'] = 'mobile';
    target['name'] = json['title'];
    target['url'] = json['url'];
    target['attached'] = !json['webSocketDebuggerUrl'];
    target['favicon_url'] = json['faviconUrl'];
    target['inspect_url'] = json['devtoolsFrontendUrl'];
    targets.push(target);
  }
  return targets;
};

/**
 * Issues synchronous json request against target device.
 * @param {string} query DevTools protocol /json query.
 * @return {?Object} Result object.
 * @private
 */
AdbDevice.prototype.queryJson_ = function(query) {
  if (!this.tcpPort)
    return null;

  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'local-xhr/' + this.tcpPort + '/json/' + query, false);
  xhr.send(null);
  if (xhr.status !== 200)
    return null;

  try {
    var result = JSON.parse(xhr.responseText);
    return result[0] ? null : JSON.parse(result[1]);
  } catch (e) {
  }
  return null;
};

function requestData() {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', 'targets-data.json', false);
  xhr.send(null);
  if (xhr.status === 200)
    return JSON.parse(xhr.responseText);
  return [];
}

function inspect(data) {
  if (data['inspect_url']) {
    window.open(data['inspect_url'], undefined,
        'location=0,width=800,height=600');
    return;
  }
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
  // Clear existing entries
  var deviceElements = document.querySelectorAll('.device');
  for (var i = 0; i < deviceElements.length; i++)
    deviceElements[i].remove();

  var devices = AdbDevice.queryDevices();
  for (var i = 0; i < devices.length; i++) {
    var device = devices[i];
    var version = device.version();
    if (!version)
      continue;

    var targets = device.targets();
    if (!targets.length)
      continue;

    var sectionElement = document.createElement('div');
    sectionElement.className = 'section device';
    var details = version['Browser'] || version['User-Agent'];
    sectionElement.textContent = device.model + ' (' + details + ')';
    var listElement = document.createElement('div');
    listElement.id = 'device-' + device.serial;
    listElement.className = 'list device';
    document.body.appendChild(sectionElement);
    document.body.appendChild(listElement);

    for (var j = 0; j < targets.length; j++) {
      addTargetToList(targets[j], 'device-' + device.serial,
            ['favicon_url', 'name', 'url']);
    }
  }
  setTimeout(populateDeviceLists, 1000);
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

document.addEventListener('DOMContentLoaded', onload);
