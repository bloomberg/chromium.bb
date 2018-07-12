// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

document.addEventListener('DOMContentLoaded', () => {

  const nameField = document.getElementById('recipeName');
  const startingURLField = document.getElementById('startingURL');
  const saveBtn = document.getElementById('saveBtn');
  const stopBtn = document.getElementById('stopBtn');
  const cancelBtn = document.getElementById('cancelBtn');
  const actionsListDiv = document.getElementById('actions');

  function createActionElement(action) {
    const actionDiv = document.createElement('div');
    actionDiv.classList.add('action');

    // The action row.
    const flexContainerDiv = document.createElement('div');
    flexContainerDiv.classList.add('h-flex-container');
    flexContainerDiv.classList.add('tooltip-trigger');
    const selectorLabel = document.createElement('h4');
    const actionLabel = document.createElement('h4');
    const iconBoxDiv = document.createElement('div');
    iconBoxDiv.classList.add('iconBox');
    iconBoxDiv.textContent = 'X';
    iconBoxDiv.addEventListener('click', (event) => {
      sendRuntimeMessageToBackgroundScript({
          type: RecorderUiMsgEnum.REMOVE_ACTION,
          action_index: action.action_index })
      .then((response) => {
        if (response) {
          actionDiv.remove();
        }
      })
      .catch((error) => {
        console.error('Unable to remove the recorded action!', error);
      });
    });
    flexContainerDiv.appendChild(selectorLabel);
    flexContainerDiv.appendChild(actionLabel);
    flexContainerDiv.appendChild(iconBoxDiv);

    // The action detail row.
    const detailsDiv = document.createElement('div');
    detailsDiv.classList.add('tooltip-body');
    const selectorDetailLabel = document.createElement('label');
    const actionDetailLabel = document.createElement('label');
    detailsDiv.appendChild(selectorDetailLabel);
    detailsDiv.appendChild(actionDetailLabel);

    selectorLabel.textContent = action.selector;
    selectorDetailLabel.textContent = action.selector;

    switch (action.type) {
      case 'autofill':
        actionLabel.textContent = 'trigger autofill';
        actionDetailLabel.textContent = `trigger autofill`;
        break;
      case 'click':
        actionLabel.textContent = 'left-click';
        actionDetailLabel.textContent = `left click element`;
        break;
      case 'hover':
        actionLabel.textContent = 'hover';
        actionDetailLabel.textContent = `hover over element`;
        break;
      case 'pressEnter':
        actionLabel.textContent = 'enter';
        actionDetailLabel.textContent = `press enter`;
        break;
      case 'select':
        actionLabel.textContent = 'select dropdown option';
        actionDetailLabel.textContent = `select option ${action.index}`;
        break;
      case 'type':
        actionLabel.textContent = 'type';
        actionDetailLabel.textContent = `type '${action.value}'`;
        break;
      case 'validateField':
        actionLabel.textContent = 'validate';
        actionDetailLabel.textContent = `check that field
            (${action.expectedAutofillType}) has the value
            '${action.expectedValue}'`;
        break;
      default:
        actionLabel.textContent = 'unknown';
        actionDetailLabel.textContent = `unknown action: ${action.type}`;
    }

    flexContainerDiv.appendChild(detailsDiv);
    actionDiv.appendChild(flexContainerDiv);
    return actionDiv;
  }

  function addAction(action) {
    actionsListDiv.appendChild(createActionElement(action));
    // Scroll to the action that the panel just appended.
    actionsListDiv.scrollTop = actionsListDiv.scrollHeight;
  }

  saveBtn.addEventListener('click', (event) => {
    sendRuntimeMessageToBackgroundScript({ type: RecorderMsgEnum.SAVE });
  });
  stopBtn.addEventListener('click', (event) => {
    sendRuntimeMessageToBackgroundScript({ type: RecorderMsgEnum.STOP });
  });
  cancelBtn.addEventListener('click', (event) => {
    sendRuntimeMessageToBackgroundScript({ type: RecorderMsgEnum.CANCEL });
  });

  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (!request) return;
    switch (request.type) {
      case RecorderUiMsgEnum.ADD_ACTION:
        addAction(request.action);
        sendResponse(true);
        break;
      default:
    }
    return false;
  });

  sendRuntimeMessageToBackgroundScript(
      { type: RecorderUiMsgEnum.GET_RECIPE })
  .then((recipe) => {
    while(actionsListDiv.hasChildNodes()) {
      actionsListDiv.removeChild(actionsListDiv.lastChild);
    }

    nameField.value = recipe.name;
    startingURLField.value = recipe.startingURL;

    for (let index = 0; index < recipe.actions.length; index++) {
      const action = recipe.actions[index];
      actionsListDiv.appendChild(createActionElement(action));
    }
  });
});
