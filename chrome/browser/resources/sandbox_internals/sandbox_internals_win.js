// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   processId: number,
 *   processType: string,
 *   name: string,
 *   metricsName: string
 * }}
 */
let BrowserHostProcess;

/**
 * @typedef {{
 *   processId: number
 * }}
 */
let RendererHostProcess;

/**
 * @typedef {{
 *   processIds: !Array<number>
 * }}
 */
let PolicyDiagnostic;

/**
 * @typedef {{
 *   browser: !Array<!BrowserHostProcess>,
 *   renderer: !Array<!RendererHostProcess>,
 *   policies: !Array<!PolicyDiagnostic>
 * }}
 */
let SandboxDiagnostics;

/**
 *  TODO(997273) Join lists into a single table with one row per process
 *  and add additional fields from Policy objects.
 */

/** @param {!SandboxDiagnostics} results */
function onGetSandboxDiagnostics(results) {
  $('raw-info').textContent =
      'browserProcesses:' + JSON.stringify(results.browser, null, 2) + '\n' +
      'rendererProcesses:' + JSON.stringify(results.renderer, null, 2) + '\n' +
      'policies:' + JSON.stringify(results.policies, null, 2);
}

document.addEventListener('DOMContentLoaded', () => {
  cr.sendWithPromise('requestSandboxDiagnostics').then(onGetSandboxDiagnostics);
});
