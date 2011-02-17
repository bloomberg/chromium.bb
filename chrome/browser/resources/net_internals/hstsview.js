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
function HSTSView(mainBoxId, queryInputId, formId, queryOutputDivId,
                  addInputId, addFormId, addCheckId,
                  deleteInputId, deleteFormId) {
  DivView.call(this, mainBoxId);

  this.queryInput_ = document.getElementById(queryInputId);
  this.addCheck_ = document.getElementById(addCheckId);
  this.addInput_ = document.getElementById(addInputId);
  this.deleteInput_ = document.getElementById(deleteInputId);
  this.queryOutputDiv_ = document.getElementById(queryOutputDivId);

  var form = document.getElementById(formId);
  form.addEventListener('submit', this.onSubmitQuery_.bind(this), false);
  form = document.getElementById(addFormId);
  form.addEventListener('submit', this.onSubmitAdd_.bind(this), false);
  form = document.getElementById(deleteFormId);
  form.addEventListener('submit', this.onSubmitDelete_.bind(this), false);

  g_browser.addHSTSObserver(this);
}

inherits(HSTSView, DivView);

HSTSView.prototype.onSubmitQuery_ = function(event) {
  g_browser.sendHSTSQuery(this.queryInput_.value);
  event.preventDefault();
};

HSTSView.prototype.onSubmitAdd_ = function(event) {
  g_browser.sendHSTSAdd(this.addInput_.value, this.addCheck_.checked);
  g_browser.sendHSTSQuery(this.addInput_.value);
  this.queryInput_.value = this.addInput_.value;
  this.addCheck_.checked = false;
  this.addInput_.value = '';
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
    s.innerText = result.error;
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
  t.innerText = hstsModeToString(result.mode);

  addTextNode(this.queryOutputDiv_, ' include_subdomains:');

  t = addNode(this.queryOutputDiv_, 'tt');
  t.innerText = result.subdomains;

  addTextNode(this.queryOutputDiv_, ' domain:');

  t = addNode(this.queryOutputDiv_, 'tt');
  t.innerText = result.domain;

  addTextNode(this.queryOutputDiv_, ' is_preloaded:');

  t = addNode(this.queryOutputDiv_, 'tt');
  t.innerText = result.preloaded;

  yellowFade(this.queryOutputDiv_);
}
