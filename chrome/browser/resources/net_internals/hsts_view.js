// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * HSTS is HTTPS Strict Transport Security: a way for sites to elect to always
 * use HTTPS. See http://dev.chromium.org/sts
 *
 * This UI allows a user to query and update the browser's list of HSTS domains.

 *  @constructor
 */
function HSTSView() {
  const mainBoxId = 'hsts-view-tab-content';
  const queryInputId = 'hsts-view-query-input';
  const formId = 'hsts-view-query-form';
  const queryOutputDivId = 'hsts-view-query-output';
  const addInputId = 'hsts-view-add-input';
  const addFormId = 'hsts-view-add-form';
  const addCheckId = 'hsts-view-check-input';
  const addPinsId = 'hsts-view-add-pins';
  const deleteInputId = 'hsts-view-delete-input';
  const deleteFormId = 'hsts-view-delete-form';

  DivView.call(this, mainBoxId);

  this.queryInput_ = $(queryInputId);
  this.addCheck_ = $(addCheckId);
  this.addInput_ = $(addInputId);
  this.addPins_ = $(addPinsId);
  this.deleteInput_ = $(deleteInputId);
  this.queryOutputDiv_ = $(queryOutputDivId);

  var form = $(formId);
  form.addEventListener('submit', this.onSubmitQuery_.bind(this), false);
  form = $(addFormId);
  form.addEventListener('submit', this.onSubmitAdd_.bind(this), false);
  form = $(deleteFormId);
  form.addEventListener('submit', this.onSubmitDelete_.bind(this), false);

  g_browser.addHSTSObserver(this);
}

inherits(HSTSView, DivView);

HSTSView.prototype.onSubmitQuery_ = function(event) {
  g_browser.sendHSTSQuery(this.queryInput_.value);
  event.preventDefault();
};

HSTSView.prototype.onSubmitAdd_ = function(event) {
  g_browser.sendHSTSAdd(this.addInput_.value,
                        this.addCheck_.checked,
                        this.addPins_.value);
  g_browser.sendHSTSQuery(this.addInput_.value);
  this.queryInput_.value = this.addInput_.value;
  this.addCheck_.checked = false;
  this.addInput_.value = '';
  this.addPins_.value = '';
  event.preventDefault();
};

HSTSView.prototype.onSubmitDelete_ = function(event) {
  g_browser.sendHSTSDelete(this.deleteInput_.value);
  this.deleteInput_.value = '';
  event.preventDefault();
};

function hstsModeToString(m) {
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

HSTSView.prototype.onHSTSQueryResult = function(result) {
  if (result.error != undefined) {
    this.queryOutputDiv_.innerHTML = '';
    s = addNode(this.queryOutputDiv_, 'span');
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

  s = addNode(this.queryOutputDiv_, 'span');
  s.innerHTML = '<b>Found</b>: mode: ';

  t = addNode(this.queryOutputDiv_, 'tt');
  t.textContent = hstsModeToString(result.mode);

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
