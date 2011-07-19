// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function closeDialog() {
  chrome.send('DialogClose', ['']);
}

function setInfobarLabel(text) {
  $('info-banner').textContent = text;
  $('info-banner').hidden = !text.length;
}

function updateControlState() {
  var allowedCookies = $('allowed-cookies');
  $('block-button').disabled = !allowedCookies.children.length ||
      !allowedCookies.selectedItem ||
      allowedCookies.selectedItem.data.type != 'origin';

  var blockedCookies = $('blocked-cookies');
  $('allow-button').disabled =
  $('allow-this-session-button').disabled = !blockedCookies.children.length ||
      !blockedCookies.selectedItem ||
      blockedCookies.selectedItem.data.type != 'origin';
}

function handleCookiesTreeChange(e) {
  updateControlState();
}

function handleBlockButtonClick(e) {
  var selected = $('allowed-cookies').selectedItem;
  if (!selected)
    return;

  chrome.send('Block', [selected.pathId]);
}

function handleAllowButtonClick(e) {
  var selected = $('blocked-cookies').selectedItem;
  if (selected)
    chrome.send('Allow', [selected.pathId]);
}

function handleAllowThisSessionButtonClick(e) {
  var selected = $('blocked-cookies').selectedItem;
  if (selected)
    chrome.send('AllowThisSession', [selected.pathId]);
}

function load() {
  ui.CookiesTree.decorate($('allowed-cookies'));
  ui.CookiesTree.decorate($('blocked-cookies'));

  chrome.send('BindCookiesTreeModel', []);

  $('allowed-cookies').addEventListener('change', handleCookiesTreeChange);
  $('blocked-cookies').addEventListener('change', handleCookiesTreeChange);

  $('block-button').addEventListener('click', handleBlockButtonClick);
  $('allow-button').addEventListener('click', handleAllowButtonClick);
  $('allow-this-session-button').addEventListener('click',
      handleAllowThisSessionButtonClick);

  $('close-button').addEventListener('click', closeDialog);

  document.oncontextmenu = function(e) {
    e.preventDefault();
  }

  document.onkeydown = function(e) {
    if (e.keyCode == 27)  // Esc
      closeDialog();
  }

  updateControlState();
}

document.addEventListener('DOMContentLoaded', load);
