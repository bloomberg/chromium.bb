// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * HSTS is HTTPS Strict Transport Security: a way for sites to elect to always
 * use HTTPS. See http://dev.chromium.org/sts
 *
 * This UI allows a user to query and update the browser's list of HSTS domains.
 */

var HSTSView = (function() {
  'use strict';

  // IDs for special HTML elements in hsts_view.html
  var MAIN_BOX_ID = 'hsts-view-tab-content';
  var QUERY_INPUT_ID = 'hsts-view-query-input';
  var FORM_ID = 'hsts-view-query-form';
  var QUERY_OUTPUT_DIV_ID = 'hsts-view-query-output';
  var ADD_INPUT_ID = 'hsts-view-add-input';
  var ADD_FORM_ID = 'hsts-view-add-form';
  var ADD_CHECK_ID = 'hsts-view-check-input';
  var ADD_PINS_ID = 'hsts-view-add-pins';
  var DELETE_INPUT_ID = 'hsts-view-delete-input';
  var DELETE_FORM_ID = 'hsts-view-delete-form';

  // We inherit from DivView.
  var superClass = DivView;

  /**
   * @constructor
   */
  function HSTSView() {
    assertFirstConstructorCall(HSTSView);

    // Call superclass's constructor.
    superClass.call(this, MAIN_BOX_ID);

    this.queryInput_ = $(QUERY_INPUT_ID);
    this.addCheck_ = $(ADD_CHECK_ID);
    this.addInput_ = $(ADD_INPUT_ID);
    this.addPins_ = $(ADD_PINS_ID);
    this.deleteInput_ = $(DELETE_INPUT_ID);
    this.queryOutputDiv_ = $(QUERY_OUTPUT_DIV_ID);

    var form = $(FORM_ID);
    form.addEventListener('submit', this.onSubmitQuery_.bind(this), false);
    form = $(ADD_FORM_ID);
    form.addEventListener('submit', this.onSubmitAdd_.bind(this), false);
    form = $(DELETE_FORM_ID);
    form.addEventListener('submit', this.onSubmitDelete_.bind(this), false);

    g_browser.addHSTSObserver(this);
  }

  // ID for special HTML element in category_tabs.html
  HSTSView.TAB_HANDLE_ID = 'tab-handle-hsts';

  cr.addSingletonGetter(HSTSView);

  HSTSView.prototype = {
    // Inherit the superclass's methods.
    __proto__: superClass.prototype,

    onSubmitQuery_: function(event) {
      g_browser.sendHSTSQuery(this.queryInput_.value);
      event.preventDefault();
    },

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

      addTextNode(this.queryOutputDiv_, ' is_preloaded:');

      t = addNode(this.queryOutputDiv_, 'tt');
      t.textContent = result.preloaded;

      addTextNode(this.queryOutputDiv_, ' pubkey_hashes:');

      t = addNode(this.queryOutputDiv_, 'tt');
      t.textContent = result.public_key_hashes;

      yellowFade(this.queryOutputDiv_);
    }
  };

  function modeToString(m) {
    if (m == 0) {
      return 'STRICT';
    } else if (m == 1) {
      return 'OPPORTUNISTIC';
    } else if (m == 2) {
      return 'SPDY';
    } else if (m == 3) {
      return 'NONE';
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
