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
  var selectedTabName = window.location.hash.slice(1) || 'devices';
  selectTab(selectedTabName + '-tab');
  initPortForwarding();
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
  removeChildren('others');

  for (var i = 0; i < data.length; i++) {
    if (data[i].type === 'page')
      addToPagesList(data[i]);
    else if (data[i].type === 'extension')
      addToExtensionsList(data[i]);
    else if (data[i].type === 'app')
      addToAppsList(data[i]);
    else
      addToOthersList(data[i]);
  }
}

function populateWorkersList(data) {
  removeChildren('workers');

  for (var i = 0; i < data.length; i++)
    addToWorkersList(data[i]);
}

function populateDeviceLists(devices) {
  var devicesDigest = JSON.stringify(devices);
  if (!devices || devicesDigest == window.devicesDigest)
    return;

  window.devicesDigest = devicesDigest;

  var containerElement = $('devices');
  containerElement.textContent = '';

  // Populate with new entries
  for (var d = 0; d < devices.length; d++) {
    var device = devices[d];

    var deviceHeader = document.createElement('div');
    deviceHeader.className = 'section';
    deviceHeader.textContent = device.adbModel;
    containerElement.appendChild(deviceHeader);

    var deviceContent = document.createElement('div');
    deviceContent.className = 'list';
    containerElement.appendChild(deviceContent);

    for (var b = 0; b < device.browsers.length; b++) {
      var browser = device.browsers[b];

      var browserHeader = document.createElement('div');
      browserHeader.className = 'small-section';
      browserHeader.textContent = browser.adbBrowserName;
      deviceContent.appendChild(browserHeader);

      var browserPages = document.createElement('div');
      browserPages.className = 'list package';
      deviceContent.appendChild(browserPages);

      for (var p = 0; p < browser.pages.length; p++) {
        addTargetToList(
            browser.pages[p], browserPages, ['faviconUrl', 'name', 'url']);
      }
    }
  }
}

function addToPagesList(data) {
  addTargetToList(data, $('pages'), ['faviconUrl', 'name', 'url']);
}

function addToExtensionsList(data) {
  addTargetToList(data, $('extensions'), ['name', 'url']);
}

function addToAppsList(data) {
  addTargetToList(data, $('apps'), ['name', 'url']);
}

function addToWorkersList(data) {
  addTargetToList(data,
                  $('workers'),
                  ['name', 'url', 'pid'],
                  true);
}

function addToOthersList(data) {
  addTargetToList(data, $('others'), ['url']);
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

function addTargetToList(data, list, properties, canTerminate) {
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


function initPortForwarding() {
  $('port-forwarding-enable').addEventListener('change', enablePortForwarding);

  $('port-forwarding-config-open').addEventListener(
      'click', openPortForwardingConfig);
  $('port-forwarding-config-close').addEventListener(
      'click', closePortForwardingConfig);
  $('port-forwarding-config-done').addEventListener(
      'click', commitPortForwardingConfig);
}

function enablePortForwarding(event) {
  chrome.send('set-port-forwarding-enabled', [event.target.checked]);
}

function handleKey(event) {
  switch (event.keyCode) {
    case 13:  // Enter
      if (event.target.nodeName == 'INPUT') {
        var line = event.target.parentNode;
        if (!line.classList.contains('fresh') ||
            line.classList.contains('empty'))
          commitPortForwardingConfig();
        else
          commitFreshLineIfValid(true /* select new line */);
      } else {
        commitPortForwardingConfig();
      }
      break;

    case 27:
      closePortForwardingConfig();
      break;
  }
}

function openPortForwardingConfig() {
  loadPortForwardingConfig(window.portForwardingConfig);

  $('port-forwarding-overlay').classList.add('open');
  document.addEventListener('keyup', handleKey);

  var freshPort = document.querySelector('.fresh .port');
  if (freshPort)
    freshPort.focus();
  else
    $('port-forwarding-config-done').focus();
}

function closePortForwardingConfig() {
  $('port-forwarding-overlay').classList.remove('open');
  document.removeEventListener('keyup', handleKey);
}

function loadPortForwardingConfig(config) {
  var list = $('port-forwarding-config-list');
  list.textContent = '';
  for (var port in config)
    list.appendChild(createConfigLine(port, config[port]));
  list.appendChild(createEmptyConfigLine());
}

function commitPortForwardingConfig() {
  if (document.querySelector(
      '.port-forwarding-pair:not(.fresh) input.invalid'))
    return;

  if (document.querySelector(
      '.port-forwarding-pair.fresh:not(.empty) input.invalid'))
    return;

  closePortForwardingConfig();
  commitFreshLineIfValid();
  var lines = document.querySelectorAll('.port-forwarding-pair');
  var config = {};
  for (var i = 0; i != lines.length; i++) {
    var line = lines[i];
    var portInput = line.querySelector('.port:not(.invalid)');
    var locationInput = line.querySelector('.location:not(.invalid)');
    if (portInput && locationInput)
      config[portInput.value] = locationInput.value;
  }
  chrome.send('set-port-forwarding-config', [config]);
}

function updatePortForwardingEnabled(enabled) {
  var checkbox = $('port-forwarding-enable');
  checkbox.checked = !!enabled;
  checkbox.disabled = false;
}

function updatePortForwardingConfig(config) {
  window.portForwardingConfig = config;
  $('port-forwarding-config-open').disabled = !config;
}

function createConfigLine(port, location) {
  var line = document.createElement('div');
  line.className = 'port-forwarding-pair';

  var portInput = createConfigField(port, 'port', 'Port', validatePort);
  line.appendChild(portInput);

  var locationInput = createConfigField(
      location, 'location', 'IP address and port', validateLocation);
  line.appendChild(locationInput);
  locationInput.addEventListener('keydown', function(e) {
    if (e.keyIdentifier == 'U+0009' &&  // Tab
        !e.ctrlKey && !e.altKey && !e.shiftKey && !e.metaKey &&
        line.classList.contains('fresh') &&
        !line.classList.contains('empty')) {
      // Tabbing forward on the fresh line, try create a new empty one.
      commitFreshLineIfValid(true);
      e.preventDefault();
    }
  });

  var lineDelete = document.createElement('div');
  lineDelete.className = 'close-button';
  lineDelete.addEventListener('click', function() {
    var newSelection = line.nextElementSibling;
    line.parentNode.removeChild(line);
    selectLine(newSelection);
  });
  line.appendChild(lineDelete);

  line.addEventListener('click', selectLine.bind(null, line));
  line.addEventListener('focus', selectLine.bind(null, line));

  checkEmptyLine(line);

  return line;
}

function validatePort(input) {
  var match = input.value.match(/^(\d+)$/);
  if (!match)
    return false;
  var port = parseInt(match[1]);
  if (port < 5000 || 10000 < port)
    return false;

  var inputs = document.querySelectorAll('input.port:not(.invalid)');
  for (var i = 0; i != inputs.length; ++i) {
    if (inputs[i] == input)
      break;
    if (parseInt(inputs[i].value) == port)
      return false;
  }
  return true;
}

function validateLocation(input) {
  var match = input.value.match(/^([a-zA-Z0-9\.]+):(\d+)$/);
  if (!match)
    return false;
  var port = parseInt(match[2]);
  return port <= 10000;
}

function createEmptyConfigLine() {
  var line = createConfigLine('', '');
  line.classList.add('fresh');
  return line;
}

function createConfigField(value, className, hint, validate) {
  var input = document.createElement('input');
  input.className = className;
  input.type = 'text';
  input.placeholder = hint;
  input.value = value;

  function checkInput() {
    if (validate(input))
      input.classList.remove('invalid');
    else
      input.classList.add('invalid');
    if (input.parentNode)
      checkEmptyLine(input.parentNode);
  }
  checkInput();

  input.addEventListener('keyup', checkInput);
  input.addEventListener('focus', function() {
    selectLine(input.parentNode);
  });

  return input;
}

function checkEmptyLine(line) {
  var inputs = line.querySelectorAll('input');
  var empty = true;
  for (var i = 0; i != inputs.length; i++) {
    if (inputs[i].value != '')
      empty = false;
  }
  if (empty)
    line.classList.add('empty');
  else
    line.classList.remove('empty');
}

function selectLine(line) {
  if (line.classList.contains('selected'))
    return;
  unselectLine();
  line.classList.add('selected');
}

function unselectLine() {
  var line = document.querySelector('.port-forwarding-pair.selected');
  if (!line)
    return;
  line.classList.remove('selected');
  commitFreshLineIfValid();
}

function commitFreshLineIfValid(opt_selectNew) {
  var line = document.querySelector('.port-forwarding-pair.fresh');
  if (line.querySelector('.invalid'))
    return;
  line.classList.remove('fresh');
  var freshLine = createEmptyConfigLine();
  line.parentNode.appendChild(freshLine);
  if (opt_selectNew)
    freshLine.querySelector('.port').focus();
}

document.addEventListener('DOMContentLoaded', onload);
