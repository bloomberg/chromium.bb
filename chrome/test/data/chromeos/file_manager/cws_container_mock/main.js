// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var DUMMY_ITEM_ID = 'DUMMY_ITEM_ID_FOR_TEST';

var origin;
var cwsContainerMock = {};

cwsContainerMock.onMessage = function (message) {
  var data = message.data;
  var source = message.source;
  origin = message.origin;

  switch (data['message']) {
    case 'initialize':
      cwsContainerMock.onInitialize(source);
      break;
  };
}

cwsContainerMock.onInitialize = function(source) {
  source.postMessage({message: 'widget_loaded'}, origin);
};

cwsContainerMock.onInstallButton = function(source) {
  source.postMessage(
      {message: 'before_install', item_id:DUMMY_ITEM_ID}, origin);
};

window.addEventListener('message', cwsContainerMock.onMessage);

function onInstallButton() {
  cwsContainerMock.onInstallButton();
};
