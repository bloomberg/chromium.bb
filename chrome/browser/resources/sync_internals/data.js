// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
var dumpToTextButton = $('dump-to-text');
var dataDump = $('data-dump');
dumpToTextButton.addEventListener('click', function(event) {
  // TODO(akalin): Add info like Chrome version, OS, date dumped, etc.

  var data = '';
  data += '======\n';
  data += 'Status\n';
  data += '======\n';
  data += JSON.stringify(chrome.sync.aboutInfo, null, 2);
  data += '\n';
  data += '\n';

  data += '=============\n';
  data += 'Notifications\n';
  data += '=============\n';
  data += JSON.stringify(chrome.sync.notifications, null, 2);
  data += '\n';
  data += '\n';

  data += '===\n';
  data += 'Log\n';
  data += '===\n';
  data += JSON.stringify(chrome.sync.log.entries, null, 2);
  data += '\n';

  dataDump.textContent = data;
});

// TODO(mmontgomery): add SPECIFICS as an opt-in.
var allFields = [
  'ID',
  'IS_UNSYNCED',
  'IS_UNAPPLIED_UPDATE',
  'BASE_VERSION',
  'BASE_VERSION_TIME',
  'SERVER_VERSION',
  'SERVER_VERSION_TIME',
  'PARENT_ID',
  'SERVER_PARENT_ID',
  'IS_DEL',
  'SERVER_IS_DEL',
  'serverModelType',
];

function versionToDateString(version) {
  // TODO(mmontgomery): ugly? Hacky? Is there a better way?
  var epochLength = Date.now().toString().length;
  var epochTime = parseInt(version.slice(0, epochLength));
  var date = new Date(epochTime);
  return date.toString();
}

function getFields(node) {
  return allFields.map(function(field) {
    var fieldVal;
    if (field == 'SERVER_VERSION_TIME') {
      var version = node['SERVER_VERSION'];
      fieldVal = versionToDateString(version);
    } if (field == 'BASE_VERSION_TIME') {
      var version = node['BASE_VERSION'];
      fieldVal = versionToDateString(version);
    } else {
      fieldVal = node[field];
    }
    return fieldVal;
  });
}

function isSelectedDatatype(node) {
  var type = node.serverModelType;
  var typeCheckbox = $(type);
  // Some types, such as 'Top level folder', appear in the list of nodes
  // but not in the list of selectable items.
  if (typeCheckbox == null) {
    return false;
  }
  return typeCheckbox.checked;
}

function makeBlobUrl(data) {
  var textBlob = new Blob([data], {type: 'octet/stream'});
  var blobUrl = window.webkitURL.createObjectURL(textBlob);
  return blobUrl;
}

function makeDownloadName() {
  // Format is sync-data-dump-$epoch-$year-$month-$day-$OS.csv.
  var now = new Date();
  var friendlyDate = [now.getFullYear(),
                      now.getMonth() + 1,
                      now.getDate()].join('-');
  var name = ['sync-data-dump',
              friendlyDate,
              Date.now(),
              navigator.platform].join('-');
  return [name, 'csv'].join('.');
}

function makeDateUserAgentHeader() {
  var now = new Date();
  var userAgent = window.navigator.userAgent;
  var dateUaHeader = [now.toISOString(), userAgent].join(',');
  return dateUaHeader;
}

function triggerDataDownload(data) {
  // Prepend a header with ISO date and useragent.
  var output = [makeDateUserAgentHeader()];
  output.push('=====');

  var aboutInfo = JSON.stringify(chrome.sync.aboutInfo, null, 2);
  output.push(aboutInfo);

  if (data != null && data.length > 0) {
    output.push('=====');
    var fieldLabels = allFields.join(',');
    output.push(fieldLabels);

    var data = data.filter(isSelectedDatatype);
    data = data.map(getFields);
    var dataAsString = data.join('\n');
    output.push(dataAsString);
  }
  output = output.join('\n');

  var anchor = $('dump-to-file-anchor');
  anchor.href = makeBlobUrl(output);
  anchor.download = makeDownloadName();
  anchor.click();
}

function createTypesCheckboxes(types) {
  var containerElt = $('node-type-checkboxes');

  types.map(function(type) {
    var div = document.createElement('div');

    var checkbox = document.createElement('input');
    checkbox.id = type;
    checkbox.type = 'checkbox';
    checkbox.checked = 'yes';
    div.appendChild(checkbox);

    var label = document.createElement('label');
    // Assigning to label.for doesn't work.
    label.setAttribute('for', type);
    label.innerText = type;
    div.appendChild(label);

    containerElt.appendChild(div);
  });
}

function populateDatatypes(childNodeSummaries) {
  var types = childNodeSummaries.map(function(n) {
    return n.type;
  });
  types = types.sort();
  createTypesCheckboxes(types);
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.sync.getRootNodeDetails(function(rootNode) {
    chrome.sync.getChildNodeIds(rootNode.id, function(childNodeIds) {
      chrome.sync.getNodeSummariesById(childNodeIds, populateDatatypes);
    });
  });
});

var dumpToFileLink = $('dump-to-file');
dumpToFileLink.addEventListener('click', function(event) {
  chrome.sync.getAllNodes(triggerDataDownload);
});
})();
