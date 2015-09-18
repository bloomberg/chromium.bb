// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Do not test orientation or hover attributes (similar to exclusions on native
// accessibility), since they can be inconsistent depending on the environment.
var RemoveUntestedStates = function(state) {
  var result = JSON.parse(JSON.stringify(state));
  delete result[StateType.horizontal];
  delete result[StateType.hovered];
  delete result[StateType.vertical];
  return result;
};

var allTests = [
  function testSimplePage() {
    var title = rootNode.docTitle;
    assertEq('Automation Tests', title);

    var state = RemoveUntestedStates(rootNode.state);
    assertEq(
        {enabled: true, focusable: true, readOnly: true},
        state);

    var children = rootNode.children;
    assertEq(RoleType.rootWebArea, rootNode.role);
    assertEq(1, children.length);
    var body = children[0];
    assertEq('body', body.htmlTag);
    state = RemoveUntestedStates(body.state);
    assertEq({enabled: true, readOnly: true}, state);

    var contentChildren = body.children;
    assertEq(3, contentChildren.length);
    var okButton = contentChildren[0];
    assertEq('Ok', okButton.name);
    state = RemoveUntestedStates(okButton.state);
    assertEq({enabled: true, focusable: true, readOnly: true}, state);
    var userNameInput = contentChildren[1];
    assertEq('Username',
             userNameInput.description);
    state = RemoveUntestedStates(userNameInput.state);
    assertEq({editable: true, enabled: true, focusable: true}, state);
    var cancelButton = contentChildren[2];
    assertEq('Cancel',
             cancelButton.name);
    state = RemoveUntestedStates(cancelButton.state);
    assertEq({enabled: true, focusable: true, readOnly: true}, state);

    // Traversal.
    assertEq(undefined, rootNode.parent);
    assertEq(rootNode, body.parent);

    assertEq(body, rootNode.firstChild);
    assertEq(body, rootNode.lastChild);

    assertEq(okButton, body.firstChild);
    assertEq(cancelButton, body.lastChild);

    assertEq(body, okButton.parent);
    assertEq(body, userNameInput.parent);
    assertEq(body, cancelButton.parent);

    assertEq(undefined, okButton.previousSibling);
    assertEq(undefined, okButton.firstChild);
    assertEq(userNameInput, okButton.nextSibling);
    assertEq(undefined, okButton.lastChild);

    assertEq(okButton, userNameInput.previousSibling);
    assertEq(cancelButton, userNameInput.nextSibling);

    assertEq(userNameInput, cancelButton.previousSibling);
    assertEq(undefined, cancelButton.nextSibling);

    chrome.test.succeed();
  },
  function testIsRoot() {
    assertTrue(rootNode.isRootNode);
    assertFalse(rootNode.firstChild.isRootNode);
    chrome.test.succeed();
  }
];

setUpAndRunTests(allTests);
