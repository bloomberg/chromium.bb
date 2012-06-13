/* Copyright (c) 2012 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Helper javascript injected whenever a DomMutationEventObserver is created.
 *
 * This script uses MutationObservers to watch for changes to the DOM, then
 * reports the event to the observer using the DomAutomationController. An
 * anonymous namespace is used to prevent conflict with other Javascript.
 *
 * Args:
 *  automation_id: Automation id used to route DomAutomationController messages.
 *  observer_id: Id of the observer who will be receiving the messages.
 *  observer_type: One of 'add', 'remove', 'change', or 'exists'.
 *  xpath: XPath used to specify the DOM node of interest.
 *  attribute: If |expected_value| is provided, check if this attribute of the
 *      DOM node matches |expected value|.
 *  expected_value: If not null, regular expression to match with the value of
 *      |attribute| after the mutation.
 */
function(automation_id, observer_id, observer_type, xpath, attribute,
         expected_value) {

  /* Raise an event for the DomMutationEventObserver. */
  function raiseEvent() {
    if (window.domAutomationController) {
      console.log("Event sent to DomEventObserver with id=" +
                  observer_id + ".");
      window.domAutomationController.sendWithId(
          automation_id, "__dom_mutation_observer__:" + observer_id);
    }
  }

  /* Calls raiseEvent if the expected node has been added to the DOM.
   *
   * Args:
   *  mutations: A list of mutation objects.
   *  observer: The mutation observer object associated with this callback.
   */
  function addNodeCallback(mutations, observer) {
    for (var j=0; j<mutations.length; j++) {
      for (var i=0; i<mutations[j].addedNodes.length; i++) {
        var node = mutations[j].addedNodes[i];
        if (xpathMatchesNode(node, xpath) &&
            nodeAttributeValueEquals(node, attribute, expected_value)) {
          raiseEvent();
          observer.disconnect();
          delete observer;
          return;
        }
      }
    }
  }

  /* Calls raiseEvent if the expected node has been removed from the DOM.
   *
   * Args:
   *  mutations: A list of mutation objects.
   *  observer: The mutation observer object associated with this callback.
   */
  function removeNodeCallback(mutations, observer) {
    var node = firstXPathNode(xpath);
    if (!node) {
      raiseEvent();
      observer.disconnect();
      delete observer;
    }
  }

  /* Calls raiseEvent if the given node has been changed to expected_value.
   *
   * Args:
   *  mutations: A list of mutation objects.
   *  observer: The mutation observer object associated with this callback.
   */
  function changeNodeCallback(mutations, observer) {
    for (var j=0; j<mutations.length; j++) {
      if (nodeAttributeValueEquals(mutations[j].target, attribute,
                                   expected_value)) {
        raiseEvent();
        observer.disconnect();
        delete observer;
        return;
      }
    }
  }

  /* Calls raiseEvent if the expected node exists in the DOM.
   *
   * Args:
   *  mutations: A list of mutation objects.
   *  observer: The mutation observer object associated with this callback.
   */
  function existsNodeCallback(mutations, observer) {
    if (findNodeMatchingXPathAndValue(xpath, attribute, expected_value)) {
      raiseEvent();
      observer.disconnect();
      delete observer;
      return;
    }
  }

  /* Return true if the xpath matches the given node.
   *
   * Args:
   *  node: A node object from the DOM.
   *  xpath: An XPath used to compare with the DOM node.
   */
  function xpathMatchesNode(node, xpath) {
    var con = document.evaluate(xpath, document, null,
        XPathResult.UNORDERED_NODE_ITERATOR_TYPE, null);
    var thisNode = con.iterateNext();
    while (thisNode) {
      if (node == thisNode) {
        return true;
      }
      thisNode = con.iterateNext();
    }
    return false;
  }

  /* Returns the first node in the DOM that matches the xpath.
   *
   * Args:
   *  xpath: XPath used to specify the DOM node of interest.
   */
  function firstXPathNode(xpath) {
    return document.evaluate(xpath, document, null,
        XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
  }

  /* Returns the first node in the DOM that matches the xpath.
   *
   * Args:
   *  xpath: XPath used to specify the DOM node of interest.
   *  attribute: The attribute to match |expected_value| against.
   *  expected_value: A regular expression to match with the node's
   *      |attribute|. If null the match always succeeds.
   */
  function findNodeMatchingXPathAndValue(xpath, attribute, expected_value) {
    var nodes = document.evaluate(xpath, document, null,
                                  XPathResult.ORDERED_NODE_ITERATOR_TYPE, null);
    var node;
    while ( (node = nodes.iterateNext()) ) {
      if (nodeAttributeValueEquals(node, attribute, expected_value))
        return node;
    }
    return null;
  }

  /* Returns true if the node's |attribute| value is matched by the regular
   * expression |expected_value|, false otherwise.
   *
   * Args:
   *  node: A node object from the DOM.
   *  attribute: The attribute to match |expected_value| against.
   *  expected_value: A regular expression to match with the node's
   *      |attribute|. If null the test always passes.
   */
  function nodeAttributeValueEquals(node, attribute, expected_value) {
    return expected_value == null ||
        (node[attribute] && RegExp(expected_value, "").test(node[attribute]));
  }

  /* Watch for a node matching xpath to be added to the DOM.
   *
   * Args:
   *  xpath: XPath used to specify the DOM node of interest.
   */
  function observeAdd(xpath) {
    window.domAutomationController.send("success");
    if (findNodeMatchingXPathAndValue(xpath, attribute, expected_value)) {
      raiseEvent();
      console.log("Matching node in DOM, assuming it was previously added.");
      return;
    }

    var obs = new WebKitMutationObserver(addNodeCallback);
    obs.observe(document,
        { childList: true,
          attributes: true,
          characterData: true,
          subtree: true});
  }

  /* Watch for a node matching xpath to be removed from the DOM.
   *
   * Args:
   *  xpath: XPath used to specify the DOM node of interest.
   */
  function observeRemove(xpath) {
    window.domAutomationController.send("success");
    if (!firstXPathNode(xpath)) {
      raiseEvent();
      console.log("No matching node in DOM, assuming it was already removed.");
      return;
    }

    var obs = new WebKitMutationObserver(removeNodeCallback);
    obs.observe(document,
        { childList: true,
          attributes: true,
          subtree: true});
  }

  /* Watch for the textContent of a node matching xpath to change to
   * expected_value.
   *
   * Args:
   *  xpath: XPath used to specify the DOM node of interest.
   */
  function observeChange(xpath) {
    var node = firstXPathNode(xpath);
    if (!node) {
      console.log("No matching node in DOM.");
      window.domAutomationController.send(
          "No DOM node matching xpath exists.");
      return;
    }
    window.domAutomationController.send("success");

    var obs = new WebKitMutationObserver(changeNodeCallback);
    obs.observe(node,
        { childList: true,
          attributes: true,
          characterData: true,
          subtree: true});
  }

  /* Watch for a node matching xpath to exist in the DOM.
   *
   * Args:
   *  xpath: XPath used to specify the DOM node of interest.
   */
  function observeExists(xpath) {
    window.domAutomationController.send("success");
    if (findNodeMatchingXPathAndValue(xpath, attribute, expected_value)) {
      raiseEvent();
      console.log("Node already exists in DOM.");
      return;
    }

    var obs = new WebKitMutationObserver(existsNodeCallback);
    obs.observe(document,
        { childList: true,
          attributes: true,
          characterData: true,
          subtree: true});
  }

  /* Interpret arguments and launch the requested observer function. */
  function installMutationObserver() {
    switch (observer_type) {
      case "add":
        observeAdd(xpath);
        break;
      case "remove":
        observeRemove(xpath);
        break;
      case "change":
        observeChange(xpath);
        break;
      case "exists":
        observeExists(xpath);
        break;
    }
    console.log("MutationObserver javscript injection completed.");
  }

  /* Ensure the DOM is loaded before attempting to create MutationObservers. */
  if (document.body) {
    installMutationObserver();
  } else {
    window.addEventListener("DOMContentLoaded", installMutationObserver, true);
  }
}
