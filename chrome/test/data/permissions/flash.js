// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function triggerPrompt() {
  document.getElementById('flash-link').click();
  window.domAutomationController.send(true);
}

function triggerPromptViaNewWindow() {
  document.getElementById('flash-new-window-link').click();
  window.domAutomationController.send(true);
}

function flashIsEnabledForPlugin(plugin) {
  plugin.addEventListener('message', function handleEvent(event) {
   if (event.data.source === 'getPowerSaverStatusResponse') {
      plugin.removeEventListener('message', handleEvent);
      window.domAutomationController.send(true);
    }
  });
  if (plugin.postMessage)
    plugin.postMessage('getPowerSaverStatus');
  else
    window.domAutomationController.send(false);
}

function flashIsEnabled() {
  flashIsEnabledForPlugin(document.getElementById('flash-object'));
}

function flashIsEnabledForPluginWithoutFallback() {
  flashIsEnabledForPlugin(
      document.getElementById('flash-object-no-fallback'));
}