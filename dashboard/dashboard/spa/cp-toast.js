/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {LitElement, html, css} from 'lit-element';
import {simpleGUID, timeout} from './utils.js';

export class CpToast extends LitElement {
  static get is() { return 'cp-toast'; }

  static get properties() {
    return {
      opened: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  static get styles() {
    return css`
      :host {
        bottom: 0;
        left: 0;
        right: 0;
        position: fixed;
      }
      :host(:not([opened])) {
        visibility: hidden;
      }
    `;
  }

  constructor() {
    super();
    this.opened = false;
  }

  render() {
    return html`<slot></slot>`;
  }

  /*
    * Autocloses after `wait` ms if open() is not called again in the interim.
    * Does not autoclose if `wait` is false.
    * `wait` can also be a Promise.
    */
  async open(wait = 10000) {
    this.opened = true;
    if (!wait) return;
    const start = this.openId_ = simpleGUID();
    if (typeof wait === 'number') wait = timeout(wait);
    await wait;
    if (this.openId_ !== start) return;
    this.opened = false;
  }
}

customElements.define(CpToast.is, CpToast);
