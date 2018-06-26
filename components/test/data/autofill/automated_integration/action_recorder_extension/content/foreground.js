// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
  let panelUi;

  function createRecordingUi() {
    panelUi = document.createElement('iframe');
    panelUi.src = chrome.runtime.getURL('content/panel.html');
    panelUi.style.border = 'none';
    panelUi.style.height = '613px';
    panelUi.style.position = 'fixed';
    panelUi.style.right = 0;
    panelUi.style.top = '100px';
    panelUi.style.width = '365px';

    // 2147483647 should be the maximum possible z index value.
    panelUi.style.zIndex = 2147483647;

    // The 'clip' and the 'overflow' properties are set to prevent css rules
    // on the main page from disabling interaction with iframes.
    // Please read the following bug for more details:
    // https://crbug.com/850700
    panelUi.style.clip = 'auto';
    panelUi.style.overflow = 'visible';

    document.body.appendChild(panelUi);
  }

  function hideUi() {
    panelUi.style.display = 'none';
  }

  function showUi() {
    panelUi.style.display = 'block';
  }

  function removeUi() {
    if (panelUi) {
      panelUi.remove();
    }
  }

  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (!request) return;
    switch (request.type) {
      case RecorderUiMsgEnum.CREATE_UI:
        createRecordingUi();
        sendResponse(true);
        break;
      case RecorderUiMsgEnum.DESTROY_UI:
        removeUi();
        sendResponse(true);
        break;
      case RecorderUiMsgEnum.HIDE_UI:
        hideUi();
        sendResponse(true);
        break;
      case RecorderUiMsgEnum.SHOW_UI:
        showUi();
        sendResponse(true);
        break;
      default:
    }
    // Return false to close the message channel. Lingering message
    // channels will prevent background scripts from unloading, causing it
    // to persist.
    return false;
  });
})();
