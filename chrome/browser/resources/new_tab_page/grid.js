// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

// Displays children in a two-dimensional grid and supports focusing children
// with arrow keys.
class GridElement extends PolymerElement {
  static get is() {
    return 'ntp-grid';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /** @type {number} */
      columns: {
        type: Number,
        value: 1,
      },
    };
  }

  /**
   * @param {!Event} e
   * @private
   */
  onKeyDown_(e) {
    if (['ArrowLeft', 'ArrowRight', 'ArrowUp', 'ArrowDown'].includes(e.key)) {
      e.preventDefault();
      const items = this.$.items.assignedElements().filter(
          (el) =>
              (!!(el.offsetWidth || el.offsetHeight ||
                  el.getClientRects().length)));
      const currentIndex = items.indexOf(e.target);
      const isRtl = window.getComputedStyle(this)['direction'] === 'rtl';
      const bottomRowColumns = items.length % this.columns;
      const direction = ['ArrowRight', 'ArrowDown'].includes(e.key) ? 1 : -1;
      const inEdgeRow = direction === 1 ?
          currentIndex >= items.length - bottomRowColumns :
          currentIndex < this.columns;
      let delta = 0;
      switch (e.key) {
        case 'ArrowLeft':
        case 'ArrowRight':
          delta = direction * (isRtl ? -1 : 1);
          break;
        case 'ArrowUp':
        case 'ArrowDown':
          delta = direction * (inEdgeRow ? bottomRowColumns : this.columns);
          break;
      }
      // Handle cases where we move to an empty space in a non-full bottom row
      // and have to jump to the next row.
      if (e.key === 'ArrowUp' && inEdgeRow &&
          currentIndex >= bottomRowColumns) {
        delta -= this.columns;
      } else if (
          e.key === 'ArrowDown' && !inEdgeRow &&
          currentIndex + delta >= items.length) {
        delta += bottomRowColumns;
      }
      const mod = function(m, n) {
        return ((m % n) + n) % n;
      };
      const newIndex = mod(currentIndex + delta, items.length);
      items[newIndex].focus();
    }

    if (['Enter', ' '].includes(e.key)) {
      e.preventDefault();
      e.stopPropagation();
      e.target.click();
    }
  }
}

customElements.define(GridElement.is, GridElement);
