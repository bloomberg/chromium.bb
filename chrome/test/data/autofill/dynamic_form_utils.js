/*
 * Copyright 2018 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

function DynamicallyChangeForm() {
  RemoveForm('form1');
  var new_form = AddNewFormAndFields();
  document.getElementsByTagName('body')[0].appendChild(new_form);
}

//* Removes the initial form. */
function RemoveForm(form_id) {
  var initial_form = document.getElementById(form_id);
  initial_form.parentNode.removeChild(initial_form);
  initial_form.innerHTML = '';
  initial_form.remove();
}

/** Adds a new form and fields for the dynamic form. */
function AddNewFormAndFields() {
  var new_form = document.createElement('form');
  new_form.setAttribute('method', 'post');
  new_form.setAttribute('action', 'https://example.com/')
  new_form.setAttribute('name', 'addr1.1');
  new_form.setAttribute('id', 'form1');

  var i = document.createElement('input');
  i.setAttribute('type', 'text');
  i.setAttribute('name', 'firstname');
  i.setAttribute('id', 'firstname');
  i.setAttribute('autocomplete', 'given-name');
  new_form.appendChild(i);

  i = document.createElement('input');
  i.setAttribute('type', 'text');
  i.setAttribute('name', 'address1');
  i.setAttribute('id', 'address1');
  i.setAttribute('autocomplete', 'address-line1');
  new_form.appendChild(i);

  i = document.createElement('select');
  i.setAttribute('name', 'state');
  i.setAttribute('id', 'state');
  i.setAttribute('autocomplete', 'region');
  i.options[0] = new Option('CA', 'CA');
  i.options[1] = new Option('MA', 'MA');
  i.options[2] = new Option('TX', 'TX');
  new_form.appendChild(i);

  i = document.createElement('input');
  i.setAttribute('type', 'text');
  i.setAttribute('name', 'city');
  i.setAttribute('id', 'city');
  i.setAttribute('autocomplete', 'locality');
  new_form.appendChild(i);

  i = document.createElement('input');
  i.setAttribute('type', 'text');
  i.setAttribute('name', 'company');
  i.setAttribute('id', 'company');
  i.setAttribute('autocomplete', 'organization');
  new_form.appendChild(i);

  i = document.createElement('input');
  i.setAttribute('type', 'text');
  i.setAttribute('name', 'email');
  i.setAttribute('id', 'email');
  i.setAttribute('autocomplete', 'email');
  new_form.appendChild(i);

  i = document.createElement('input');
  i.setAttribute('type', 'text');
  i.setAttribute('name', 'phone');
  i.setAttribute('id', 'phone');
  i.setAttribute('autocomplete', 'tel');
  new_form.appendChild(i);

  return new_form;
}