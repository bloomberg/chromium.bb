// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

localStrings = new LocalStrings();

// UTF8 sequence for an arrow triangle pointing down.
kExpandedArrow = "\u25BE";
// UTF8 sequence for an arrow triangle pointing right.
kCollapsedArrow = "\u25B8";

/**
 * Requests the list of sessions from the backend.
 */
function requestSessions() {
  chrome.send('requestSessionList', [])
}

/**
 * Expands or collapses the specified list.
 */
function toggleExpandedState(div) {
  if (div.textContent.indexOf(kExpandedArrow) != -1) {
    div.textContent = div.textContent.replace(kExpandedArrow, kCollapsedArrow);
    div.parentNode.querySelector(".indent").hidden = true;
  } else {
    div.textContent = div.textContent.replace(kCollapsedArrow, kExpandedArrow);
    div.parentNode.querySelector(".indent").hidden = false;
  }
}

/**
 * Creates a div element to serve as an expandable/collapsable
 * title for a list of windows or tabs.
 */
function createTitleDiv(className, textContent) {
  var div = document.createElement('div');
  div.className = className + ' expandable';
  div.textContent = kExpandedArrow + ' ' + textContent;
  div.onclick = Function('toggleExpandedState(this)');
  return div;
}

/**
 * Utility function to call |transformFn| on each element of |from|
 * and add them to a div which is then added to |container|.
 */
function addItems(container, from, transformFn) {
  var divBlock = document.createElement('div');
  divBlock.className = 'indent';
  for (var i = 0; i < from.length; i++) {
    divBlock.appendChild(transformFn(from[i]));
  }
  container.appendChild(divBlock);
}

/**
 * Transforms a tab into an HTML element.
 */
function transformTab(tab) {
  var tabItem = document.createElement('a');
  tabItem.setAttribute('href', tab.url);
  tabItem.textContent = tab.title;
  tabItem.className = 'tab-link';
  tabItem.style.backgroundImage = url('chrome://favicon/' + tab.url);
  return tabItem;
}

/**
 * Transforms a window into an HTML element.
 */
function transformWindow(window) {
  var windowDiv = document.createElement('div');
  windowDiv.className = "window";
  windowDiv.appendChild(createTitleDiv('window-title', 'Window'));
  addItems(windowDiv, window.tabs, transformTab);
  return windowDiv;
}

/**
 * Transforms a session into an HTML element.
 */
function transformSession(session) {
  var sessionDiv = document.createElement('div');
  var sessionName = session.name.length == 0 ? 'Session' : session.name;
  sessionDiv.className = "session";
  sessionDiv.appendChild(createTitleDiv('session-title', sessionName));
  addItems(sessionDiv, session.windows, transformWindow);
  return sessionDiv;
}

/**
 * Creates the UI for the sessions tree.
 */
function createSessionTreeUI(sessionList) {
  $('sessions-summary-text').textContent =
      localStrings.getStringF('sessionsCountFormat', sessionList.length);

  var sessionSection = $('session-list');

  // Clear any previous list.
  sessionSection.textContent = '';

  var sectionDiv = document.createElement('div');
  addItems(sectionDiv, sessionList, transformSession);
  sessionSection.appendChild(sectionDiv);

  $('no-sessions').hidden = sessionList.length != 0;
}

/**
 * Creates the UI for the "magic" list.
 */
function createMagicListUI(magicList) {
  $('magic-summary-text').textContent =
      localStrings.getStringF('magicCountFormat', magicList.length);

  var magicSection = $('magic-list');

  // Clear any previous list.
  magicSection.textContent = '';

  var sectionDiv = document.createElement('div');
  addItems(sectionDiv, magicList, transformTab);
  magicSection.appendChild(sectionDiv);

  $('no-magic').hidden = magicList.length != 0;
}

/**
 * Callback from backend with the list of sessions. Builds the UI.
 * @param {array} sessionList The list of sessions.
 * @param {array} magicList List of "interesting" tabs.
 */
function updateSessionList(sessionList, magicList) {
  createSessionTreeUI(sessionList);
  createMagicListUI(magicList);
}

document.addEventListener('DOMContentLoaded', requestSessions);
