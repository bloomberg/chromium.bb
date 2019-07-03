/* Copyright 2019 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

function iterateElementDeeply(element, cb) {
  cb(element);

  if (element.shadowRoot &&
      element.shadowRoot !== element &&
      iterateElementDeeply(element.shadowRoot, cb)) {
    // Some elements, most notably Polymer template dom-repeat='...'
    // elements, are their own shadow root. Make sure that we avoid infinite
    // recursion by avoiding these elements.
    return true;
  }
  for (let i = 0; i < element.children.length; i++) {
    if (iterateElementDeeply(element.children[i], cb)) {
      return true;
    }
  }

  return false;
}

export function findElements(root, predicate) {
  const foundElements = [];
  iterateElementDeeply(root, element => {
    if (element.matches && predicate(element)) foundElements.push(element);
  });
  return foundElements;
}
