/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import './cp-input.js';
import OptionGroup from './option-group.js';
import {ElementBase, STORE} from './element-base.js';
import {UPDATE} from './simple-redux.js';
import {get} from '@polymer/polymer/lib/utils/path.js';
import {html} from '@polymer/polymer/polymer-element.js';
import {isElementChildOf, setImmutable} from './utils.js';

export default class MenuInput extends ElementBase {
  static get is() { return 'menu-input'; }

  static get properties() {
    return {
      statePath: String,
      ...OptionGroup.properties,
      label: String,
      focusTimestamp: Number,
      largeDom: Boolean,
      hasBeenOpened: Boolean,
      isFocused: Boolean,
      required: Boolean,
      requireSingle: Boolean,
    };
  }

  static buildState(options = {}) {
    return {
      ...OptionGroup.buildState(options),
      alwaysEnabled: options.alwaysEnabled !== false,
      hasBeenOpened: false,
      label: options.label || '',
      requireSingle: options.requireSingle || false,
      required: options.required || false,
    };
  }

  static get template() {
    return html`
      <style>
        :host {
          display: block;
          padding-top: 12px;
        }

        #clear {
          color: var(--neutral-color-dark, grey);
          cursor: pointer;
          flex-shrink: 0;
          height: var(--icon-size, 1em);
          width: var(--icon-size, 1em);
        }

        #menu {
          background-color: var(--background-color, white);
          box-shadow: var(--elevation-2);
          max-height: 600px;
          outline: none;
          overflow: auto;
          padding-right: 8px;
          position: absolute;
          z-index: var(--layer-menu, 100);
        }

        #bottom {
          display: flex;
        }
      </style>

      <cp-input
          id="input"
          autofocus="[[isFocused]]"
          error$="[[!isValid_(selectedOptions, alwaysEnabled, options)]]"
          disabled="[[isDisabled_(alwaysEnabled, options)]]"
          label="[[label]]"
          value="[[getInputValue_(isFocused, query, selectedOptions)]]"
          on-blur="onBlur_"
          on-focus="onFocus_"
          on-keyup="onKeyup_">
        <iron-icon
            id="clear"
            hidden$="[[isEmpty_(selectedOptions)]]"
            icon="cp:cancel"
            title="clear"
            alt="clear"
            on-click="onClear_">
        </iron-icon>
      </cp-input>

      <div id="menu" tabindex="0">
        <iron-collapse opened="[[isFocused]]">
          <slot name="top"></slot>
          <div id="bottom">
            <slot name="left"></slot>
            <option-group
                state-path="[[statePath]]"
                root-state-path="[[statePath]]">
            </option-group>
          </div>
        </iron-collapse>
      </div>
    `;
  }

  connectedCallback() {
    super.connectedCallback();
    this.observeIsFocused_();
  }

  stateChanged(rootState) {
    if (!this.statePath) return;
    this.largeDom = rootState.largeDom;
    this.rootFocusTimestamp = rootState.focusTimestamp;
    this.setProperties(get(rootState, this.statePath));
    const isFocused = (this.focusTimestamp || false) &&
      (rootState.focusTimestamp === this.focusTimestamp);
    const focusChanged = (isFocused !== this.isFocused);
    this.isFocused = isFocused;
    if (focusChanged) this.observeIsFocused_();
  }

  async observeIsFocused_() {
    if (this.isFocused) {
      this.$.input.focus();
    } else {
      this.$.input.blur();
    }
  }

  isDisabled_(alwaysEnabled, options) {
    return !alwaysEnabled && options && (options.length === 0);
  }

  isValid_(selectedOptions, alwaysEnabled, options) {
    if (this.isDisabled_(alwaysEnabled, options)) return true;
    if (!this.required) return true;
    if (!this.requireSingle && !this.isEmpty_(selectedOptions)) return true;
    if (this.requireSingle && (selectedOptions.length === 1)) return true;
    return false;
  }

  getInputValue_(isFocused, query, selectedOptions) {
    return MenuInput.inputValue(isFocused, query, selectedOptions);
  }

  async onFocus_(event) {
    if (this.isFocused) return;
    MenuInput.focus(this.statePath);
  }

  async onBlur_(event) {
    if (isElementChildOf(event.relatedTarget, this)) {
      this.$.input.focus();
      return;
    }
    STORE.dispatch(UPDATE(this.statePath, {
      focusTimestamp: window.performance.now(),
      query: '',
    }));
  }

  async onKeyup_(event) {
    if (event.key === 'Escape') {
      this.$.input.blur();
      return;
    }
    STORE.dispatch(UPDATE(this.statePath, {query: event.target.value}));
    this.dispatchEvent(new CustomEvent('input-keyup', {
      detail: {
        key: event.key,
        value: this.query,
      },
    }));
  }

  async onClear_(event) {
    STORE.dispatch(UPDATE(this.statePath, {query: '', selectedOptions: []}));
    MenuInput.focus(this.statePath);
    this.dispatchEvent(new CustomEvent('clear'));
    this.dispatchEvent(new CustomEvent('option-select', {
      bubbles: true,
      composed: true,
    }));
  }

  static focus(inputStatePath) {
    STORE.dispatch({
      type: MenuInput.reducers.focus.name,
      // NOT "statePath"! statePathReducer would mess that up.
      inputStatePath,
    });
  }

  static blurAll() {
    STORE.dispatch({type: MenuInput.reducers.focus.name});
  }
}

MenuInput.inputValue = (isFocused, query, selectedOptions) => {
  if (isFocused) return query;
  if (selectedOptions === undefined) return '';
  if (selectedOptions.length === 0) return '';
  if (selectedOptions.length === 1) return selectedOptions[0];
  return `[${selectedOptions.length} selected]`;
};

MenuInput.reducers = {
  focus: (rootState, {inputStatePath}, rootStateAgain) => {
    const focusTimestamp = window.performance.now();
    rootState = {...rootState, focusTimestamp};
    if (!inputStatePath) return rootState; // Blur all menu-inputs
    return setImmutable(rootState, inputStatePath, inputState => {
      return {...inputState, focusTimestamp, hasBeenOpened: true};
    });
  },
};

ElementBase.register(MenuInput);
