// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(dbeam): should these be global like this?
var Page = cr.ui.pageManager.Page;
var PageManager = cr.ui.pageManager.PageManager;
var BluetoothPairing = options.BluetoothPairing;

/** @override */
PageManager.closeOverlay = function() {
  chrome.send('dialogClose');
};

/**
 * Listener for the |beforeunload| event.
 * TODO(dbeam): probably ought to be using addEventListener() instead.
 */
window.onbeforeunload = function() {
  PageManager.willClose();
};

/**
 * Override calls from BluetoothOptionsHandler.
 */
cr.define('options', function() {
  function BluetoothOptions() {}

  BluetoothOptions.startDeviceDiscovery = function() {};
  BluetoothOptions.updateDiscoveryState = function() {};
  BluetoothOptions.dismissOverlay = function() {
    PageManager.closeOverlay();
  };

  return {BluetoothOptions: BluetoothOptions};
});

/**
 * DOMContentLoaded handler, sets up the page.
 */
function load() {
  if (cr.isChromeOS)
    document.documentElement.setAttribute('os', 'chromeos');

  // Decorate the existing elements in the document.
  cr.ui.decorate('input[pref][type=checkbox]', options.PrefCheckbox);
  cr.ui.decorate('input[pref][type=number]', options.PrefNumber);
  cr.ui.decorate('input[pref][type=radio]', options.PrefRadio);
  cr.ui.decorate('input[pref][type=range]', options.PrefRange);
  cr.ui.decorate('select[pref]', options.PrefSelect);
  cr.ui.decorate('input[pref][type=text]', options.PrefTextField);
  cr.ui.decorate('input[pref][type=url]', options.PrefTextField);

  // TODO(ivankr): remove when http://crosbug.com/20660 is resolved.
  var inputs = document.querySelectorAll('input[pref]');
  for (var i = 0, el; el = inputs[i]; i++) {
    el.addEventListener('keyup', function(e) {
      cr.dispatchSimpleEvent(this, 'change');
    });
  }

  chrome.send('coreOptionsInitialize');

  var pairingPage = new Page('bluetooth', '', 'bluetooth-container');
  PageManager.register(pairingPage);
  PageManager.registerOverlay(BluetoothPairing.getInstance(), pairingPage);

  // Since this UI is used to host a BluetoothPairing dialog we need to add
  // an onPairing observer. (Normally that would be added in browser_options).
  chrome.bluetoothPrivate.onPairing.addListener(
      BluetoothPairing.onBluetoothPairingEvent);

  // Show the BluetoothPairing dialog.
  var args = JSON.parse(chrome.getVariableValue('dialogArguments'));
  var device = /** @type {!chrome.bluetooth.Device} */ (args);
  BluetoothPairing.connect(device, true);
}

document.addEventListener('DOMContentLoaded', load);
