// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var group;
var h1;
var p1;
var link;
var main;
var p2;
var p3;
var anonGroup;
var okButton;
var cancelButton;

function initializeNodes(rootNode) {
  group = rootNode.firstChild;
  assertEq(RoleType.group, group.role);

  h1 = group.firstChild;
  assertEq(RoleType.heading, h1.role);
  assertEq(1, h1.attributes.hierarchicalLevel);

  p1 = group.lastChild;
  assertEq(RoleType.paragraph, p1.role);

  link = p1.children[1];
  assertEq(RoleType.link, link.role);

  main = rootNode.children[1];
  assertEq(RoleType.main, main.role);

  p2 = main.firstChild;
  assertEq(RoleType.paragraph, p2.role);

  p3 = main.lastChild;
  assertEq(RoleType.paragraph, p3.role);

  anonGroup = rootNode.lastChild;
  assertEq(RoleType.group, anonGroup.role);

  okButton = anonGroup.firstChild;
  assertEq(RoleType.button, okButton.role);
  assertEq('Ok', okButton.name);
  assertFalse(StateType.enabled in okButton.state);

  cancelButton = anonGroup.lastChild;
  assertEq(RoleType.button, cancelButton.role);
  assertEq('Cancel', cancelButton.name);
  assertTrue(StateType.enabled in cancelButton.state);
}

var allTests = [
  function testFindByRole() {
    initializeNodes(rootNode);

    // Should find the only instance of this role.
    assertEq(h1, rootNode.find({ role: RoleType.heading}));
    assertEq([h1], rootNode.findAll({ role: RoleType.heading}));

    // find should find first instance only.
    assertEq(okButton, rootNode.find({ role: RoleType.button }));
    assertEq(p1, rootNode.find({ role: RoleType.paragraph }));

    // findAll should find all instances.
    assertEq([okButton, cancelButton],
             rootNode.findAll({ role: RoleType.button }));
    assertEq([p1, p2, p3], rootNode.findAll({ role: RoleType.paragraph }));

    // No instances: find should return null; findAll should return empty array.
    assertEq(null, rootNode.find({ role: RoleType.checkbox }));
    assertEq([], rootNode.findAll({ role: RoleType.checkbox }));

    // Calling from node should search only its subtree.
    assertEq(p1, group.find({ role: RoleType.paragraph }));
    assertEq(p2, main.find({ role: RoleType.paragraph }));
    assertEq([p2, p3], main.findAll({ role: RoleType.paragraph }));

    // Unlike querySelector, can search from an anonymous group without
    // unexpected results.
    assertEq(okButton, anonGroup.find({ role: RoleType.button }));
    assertEq([okButton, cancelButton],
             anonGroup.findAll({ role: RoleType.button }));
    assertEq(null, anonGroup.find({ role: RoleType.heading }));

    chrome.test.succeed();
  },

  function testFindByStates() {
    initializeNodes(rootNode);

    // Find all focusable elements (disabled button is not focusable).
    assertEq(link, rootNode.find({ state: { focusable: true }}));
    assertEq([link, cancelButton],
             rootNode.findAll({ state: { focusable: true }}));

    // Find disabled buttons.
    assertEq(okButton, rootNode.find({ role: RoleType.button,
                                       state: { enabled: false }}));
    assertEq([okButton], rootNode.findAll({ role: RoleType.button,
                                            state: { enabled: false }}));

    // Find disabled buttons within a portion of the tree.
    assertEq(okButton, anonGroup.find({ role: RoleType.button,
                                       state: { enabled: false }}));
    assertEq([okButton], anonGroup.findAll({ role: RoleType.button,
                                            state: { enabled: false }}));

    // Find enabled buttons.
    assertEq(cancelButton, rootNode.find({ role: RoleType.button,
                                           state: { enabled: true }}));
    assertEq([cancelButton], rootNode.findAll({ role: RoleType.button,
                                                state: { enabled: true }}));
    chrome.test.succeed();
  },

  function testFindByAttribute() {
    initializeNodes(rootNode);

    // Find by name attribute.
    assertEq(okButton, rootNode.find({ attributes: { name: 'Ok' }}));
    assertEq(cancelButton, rootNode.find({ attributes: { name: 'Cancel' }}));

    // String attributes must be exact match unless a regex is used.
    assertEq(null, rootNode.find({ attributes: { name: 'Canc' }}));
    assertEq(null, rootNode.find({ attributes: { name: 'ok' }}));

    // Find by value attribute - regexp.
    var query = { attributes: { value: /relationship/ }};
    assertEq(p2, rootNode.find(query).parent);

    // Find by role and hierarchicalLevel attribute.
    assertEq(h1, rootNode.find({ role: RoleType.heading,
                                 attributes: { hierarchicalLevel: 1 }}));
    assertEq([], rootNode.findAll({ role: RoleType.heading,
                                    attributes: { hierarchicalLevel: 2 }}));

    // Searching for an attribute which no element has fails.
    assertEq(null, rootNode.find({ attributes: { charisma: 12 } }));

    // Searching for an attribute value of the wrong type fails (even if the
    // value is equivalent).
    assertEq(null, rootNode.find({ role: RoleType.heading,
                                   attributes: { hierarchicalLevel: true }} ));

    chrome.test.succeed();
  },

  function testMatches() {
    initializeNodes(rootNode);
    assertTrue(h1.matches({ role: RoleType.heading }),
               'h1 should match RoleType.heading');
    assertTrue(h1.matches({ role: RoleType.heading,
                            attributes: { hierarchicalLevel: 1 } }),
               'h1 should match RoleType.heading and hierarchicalLevel: 1');
    assertFalse(h1.matches({ role: RoleType.heading,
                             state: { focusable: true },
                             attributes: { hierarchicalLevel: 1 } }),
               'h1 should not match focusable: true');
    assertTrue(h1.matches({ role: RoleType.heading,
                            state: { focusable: false },
                            attributes: { hierarchicalLevel: 1 } }),
               'h1 should match focusable: false');

    var p2StaticText = p2.firstChild;
    assertTrue(
        p2StaticText.matches({ role: RoleType.staticText,
                               attributes: { value: /relationship/ } }),
        'p2StaticText should match value: /relationship/ (regex match)');
    assertFalse(
        p2StaticText.matches({ role: RoleType.staticText,
                               attributes: { value: 'relationship' } }),
      'p2 should not match value: \'relationship');

    chrome.test.succeed();
  }
];

setUpAndRunTests(allTests, 'complex.html');
