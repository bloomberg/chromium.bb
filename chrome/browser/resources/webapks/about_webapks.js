// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   shortName: string,
 *   packageName: string,
 *   shellApkVersion: number,
 *   versionCode: number
 * }}
 */
var WebApkInfo;

/**
 * Creates and returns an element (with |text| as content) assigning it the
 * |className| class.
 *
 * @param {string} text Text to be shown in the span.
 * @param {string} className Class to be assigned to the new element.
 * @return {Element} The created element.
 */
function createSpanWithTextAndClass(text, className) {
  var element = createElementWithClassName('span', className);
  element.textContent = text;
  return element;
}

/**
 * Callback from the backend with the information of a WebAPK to display.
 * This will be called once for each WebAPK available on the device and each
 * one will be appended at the end of the other.
 *
 * @param {!Array<WebApkInfo>} webApkList List of objects with information about
 * WebAPKs installed.
 */
function returnWebApksInfo(webApkList) {
  for (let webApkInfo of webApkList) {
    addWebApk(webApkInfo);
  }
}

/**
 * Adds a new entry to the page with the information of a WebAPK.
 *
 * @param {WebApkInfo} webApkInfo Information about an installed WebAPK.
 */
function addWebApk(webApkInfo) {
  var webApkList = $('webapk-list');

  webApkList.appendChild(
      createSpanWithTextAndClass(webApkInfo.shortName, 'app-name'));

  webApkList.appendChild(
      createSpanWithTextAndClass('Package name: ', 'app-property-label'));
  webApkList.appendChild(document.createTextNode(webApkInfo.packageName));

  webApkList.appendChild(document.createElement('br'));
  webApkList.appendChild(createSpanWithTextAndClass(
      'Shell APK version: ', 'app-property-label'));
  webApkList.appendChild(document.createTextNode(webApkInfo.shellApkVersion));

  webApkList.appendChild(document.createElement('br'));
  webApkList.appendChild(
      createSpanWithTextAndClass('Version code: ', 'app-property-label'));
  webApkList.appendChild(document.createTextNode(webApkInfo.versionCode));
}

document.addEventListener('DOMContentLoaded', function() {
  chrome.send('requestWebApksInfo');
});
