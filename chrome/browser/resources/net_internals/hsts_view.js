// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * HSTS is HTTPS Strict Transport Security: a way for sites to elect to always
 * use HTTPS. See http://dev.chromium.org/sts
 *
 * This UI allows a user to query and update the browser's list of HSTS domains.
 * It also allows users to query and update the browser's list of public key
 * pins.
 */

var HSTSView = (function() {
  'use strict';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function HSTSView() {
    assertFirstConstructorCall(HSTSView);

    // Call superclass's constructor.
    superClass.call(this, HSTSView.MAIN_BOX_ID);

    this.addInput_ = $(HSTSView.ADD_INPUT_ID);
    this.addCheck_ = $(HSTSView.ADD_CHECK_ID);
    this.addPins_ = $(HSTSView.ADD_PINS_ID);
    this.deleteInput_ = $(HSTSView.DELETE_INPUT_ID);
    this.queryInput_ = $(HSTSView.QUERY_INPUT_ID);
    this.queryOutputDiv_ = $(HSTSView.QUERY_OUTPUT_DIV_ID);

    var form = $(HSTSView.ADD_FORM_ID);
    form.addEventListener('submit', this.onSubmitAdd_.bind(this), false);

    form = $(HSTSView.DELETE_FORM_ID);
    form.addEventListener('submit', this.onSubmitDelete_.bind(this), false);

    form = $(HSTSView.QUERY_FORM_ID);
    form.addEventListener('submit', this.onSubmitQuery_.bind(this), false);

    g_browser.addHSTSObserver(this);
  }

  // ID for special HTML element in category_tabs.html
  HSTSView.TAB_HANDLE_ID = 'tab-handle-hsts';

  // IDs for special HTML elements in hsts_view.html
  HSTSView.MAIN_BOX_ID = 'hsts-view-tab-content';
  HSTSView.ADD_INPUT_ID = 'hsts-view-add-input';
  HSTSView.ADD_CHECK_ID = 'hsts-view-check-input';
  HSTSView.ADD_PINS_ID = 'hsts-view-add-pins';
  HSTSView.ADD_FORM_ID = 'hsts-view-add-form';
  HSTSView.ADD_SUBMIT_ID = 'hsts-view-add-submit';
  HSTSView.DELETE_INPUT_ID = 'hsts-view-delete-input';
  HSTSView.DELETE_FORM_ID = 'hsts-view-delete-form';
  HSTSView.DELETE_SUBMIT_ID = 'hsts-view-delete-submit';
  HSTSView.QUERY_INPUT_ID = 'hsts-view-query-input';
  HSTSView.QUERY_OUTPUT_DIV_ID = 'hsts-view-query-output';
  HSTSView.QUERY_FORM_ID = 'hsts-view-query-form';
  HSTSView.QUERY_SUBMIT_ID = 'hsts-view-query-submit';

  cr.addSingletonGetter(HSTSView);

  HSTSView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onSubmitAdd_: function(event) {
      g_browser.sendHSTSAdd(this.addInput_.value,
                            this.addCheck_.checked,
                            this.addPins_.value);
      g_browser.sendHSTSQuery(this.addInput_.value);
      this.queryInput_.value = this.addInput_.value;
      this.addCheck_.checked = false;
      this.addInput_.value = '';
      this.addPins_.value = '';
      event.preventDefault();
    },

    onSubmitDelete_: function(event) {
      g_browser.sendHSTSDelete(this.deleteInput_.value);
      this.deleteInput_.value = '';
      event.preventDefault();
    },

    onSubmitQuery_: function(event) {
      g_browser.sendHSTSQuery(this.queryInput_.value);
      event.preventDefault();
    },

    onHSTSQueryResult: function(result) {
      if (result.error != undefined) {
        this.queryOutputDiv_.innerHTML = '';
        var s = addNode(this.queryOutputDiv_, 'span');
        s.textContent = result.error;
        s.style.color = 'red';
        yellowFade(this.queryOutputDiv_);
        return;
      }

      if (result.result == false) {
        this.queryOutputDiv_.innerHTML = '<b>Not found</b>';
        yellowFade(this.queryOutputDiv_);
        return;
      }

      this.queryOutputDiv_.innerHTML = '';

      var s = addNode(this.queryOutputDiv_, 'span');
      s.innerHTML = '<b>Found</b>: mode: ';

      var t = addNode(this.queryOutputDiv_, 'tt');
      t.textContent = modeToString(result.mode);

      addTextNode(this.queryOutputDiv_, ' include_subdomains:');

      t = addNode(this.queryOutputDiv_, 'tt');
      t.textContent = result.subdomains;

      addTextNode(this.queryOutputDiv_, ' domain:');

      t = addNode(this.queryOutputDiv_, 'tt');
      t.textContent = result.domain;

      addTextNode(this.queryOutputDiv_, ' pubkey_hashes:');

      t = addNode(this.queryOutputDiv_, 'tt');

      // |public_key_hashes| is an old synonym for what is now
      // |preloaded_spki_hashes|, which in turn is a legacy synonym for
      // |static_spki_hashes|. Look for all three, and also for
      // |dynamic_spki_hashes|.
      if (typeof result.public_key_hashes === 'undefined')
        result.public_key_hashes = '';
      if (typeof result.preloaded_spki_hashes === 'undefined')
        result.preloaded_spki_hashes = '';
      if (typeof result.static_spki_hashes === 'undefined')
        result.static_spki_hashes = '';
      if (typeof result.dynamic_spki_hashes === 'undefined')
        result.dynamic_spki_hashes = '';

      var hashes = [];
      if (result.public_key_hashes)
        hashes.push(result.public_key_hashes);
      if (result.preloaded_spki_hashes)
        hashes.push(result.preloaded_spki_hashes);
      if (result.static_spki_hashes)
        hashes.push(result.static_spki_hashes);
      if (result.dynamic_spki_hashes)
        hashes.push(result.dynamic_spki_hashes);

      t.textContent = hashes.join(',');
      yellowFade(this.queryOutputDiv_);
    }
  };

  function modeToString(m) {
    // These numbers must match those in
    // TransportSecurityState::DomainState::UpgradeMode.
    if (m == 0) {
      return 'STRICT';
    } else if (m == 1) {
      return 'OPPORTUNISTIC';
    } else {
      return 'UNKNOWN';
    }
  }

  function yellowFade(element) {
    element.style.webkitTransitionProperty = 'background-color';
    element.style.webkitTransitionDuration = '0';
    element.style.backgroundColor = '#fffccf';
    setTimeout(function() {
      element.style.webkitTransitionDuration = '1000ms';
      element.style.backgroundColor = '#fff';
    }, 0);
  }

  return HSTSView;
})();
