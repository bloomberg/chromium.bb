// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {AuthCompletedCredentials} from '../../gaia_auth_host/authenticator.m.js';

import {EduCoexistenceFlowResult, EduLoginParams, ParentAccount} from './edu_login_util.js';

/** @interface */
export class EduAccountLoginBrowserProxy {
  /**
   * Sends 'updateEduCoexistenceFlowResult' request with provided value.
   * @param {EduCoexistenceFlowResult} result
   */
  updateEduCoexistenceFlowResult(result) {}

  /**
   * Send 'getParents' request to the handler. The promise will be resolved
   * with the list of parents (Array<ParentAccount>).
   * @return {Promise<Array<ParentAccount>>}
   */
  getParents() {}

  /**
   * Send parent credentials to get the reauth proof token. The promise will be
   * resolved with the RAPT.
   *
   * @param {ParentAccount} parent
   * @param {String} password parent password
   * @return {Promise<string>}
   */
  parentSignin(parent, password) {}

  /** Send 'initialize' message to prepare for starting auth. */
  loginInitialize() {}

  /**
   * Send 'authExtensionReady' message to handle tasks after auth extension
   * loads.
   */
  authExtensionReady() {}

  /**
   * Send 'switchToFullTab' message to switch the UI from a constrained dialog
   * to a full tab.
   * @param {!string} url
   */
  switchToFullTab(url) {}

  /**
   * Send 'completeLogin' message to complete login.
   * @param {!AuthCompletedCredentials} credentials
   * @param {!EduLoginParams} eduLoginParams
   */
  completeLogin(credentials, eduLoginParams) {}

  /** Send 'dialogClose' message to close the login dialog. */
  dialogClose() {}
}

/**
 * @implements {EduAccountLoginBrowserProxy}
 */
export class EduAccountLoginBrowserProxyImpl {
  /** @override */
  updateEduCoexistenceFlowResult(result) {
    chrome.send('updateEduCoexistenceFlowResult', [result]);
  }

  /** @override */
  getParents() {
    return sendWithPromise('getParents');
  }

  /** @override */
  parentSignin(parent, password) {
    return sendWithPromise('parentSignin', parent, password);
  }

  /** @override */
  loginInitialize() {
    chrome.send('initialize');
  }

  /** @override */
  authExtensionReady() {
    chrome.send('authExtensionReady');
  }

  /** @override */
  switchToFullTab(url) {
    chrome.send('switchToFullTab', [url]);
  }

  /** @override */
  completeLogin(credentials, eduLoginParams) {
    chrome.send('completeLogin', [credentials, eduLoginParams]);
  }

  /** @override */
  dialogClose() {
    chrome.send('dialogClose');
  }
}

addSingletonGetter(EduAccountLoginBrowserProxyImpl);
