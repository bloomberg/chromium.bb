/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {PolymerElement, html} from '@polymer/polymer/polymer-element.js';

export default class RaisedButton extends PolymerElement {
  static get is() { return 'raised-button'; }

  static get template() {
    return html`
      <style>
        :host {
          align-items: center;
          background-color: var(--primary-color-light, lightblue);
          background-position: center;
          border-radius: 24px;
          border: 0px none hsl(231, 50%, 50%);
          box-shadow: var(--elevation-1);
          color: var(--primary-color-dark, blue);
          cursor: pointer;
          display: flex;
          height: var(--icon-size, 24px);
          justify-content: center;
          margin: 4px 8px;
          padding: 4px 8px;
          text-transform: uppercase;
          transition: background var(--transition-long, 1s);
          user-select: none;
        }
        * {
          flex-grow: 1
        }
        :host([disabled]) {
          background-color: var(--neutral-color-light, lightgrey);
          box-shadow: none;
          color: var(--neutral-color-dark, grey);
          cursor: auto;
          pointer-events: none;
        }
        :host(:hover) {
          background: var(--primary-color-light, lightgrey) radial-gradient(
            circle, transparent 1%, var(--primary-color-light, lightgrey) 1%)
            center/15000%;
        }
        :host(:active) {
          background-color: var(--primary-color-medium, dodgerblue);
          background-size: 100%;
          transition: background 0s;
        }
      </style>

      <slot></slot>
    `;
  }
}
customElements.define(RaisedButton.is, RaisedButton);
