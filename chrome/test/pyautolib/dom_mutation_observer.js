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
 *  observer_type: One of 'add', 'remove', or 'change'.
 *  pattern: Pattern used to select the DOM node of interest.
 *  ptype: Type of |pattern|, either 'xpath' or 'css'.
 *  expected_value: If not null, regular expression matching text contents
 *      expected after the mutation.
 */

function(automation_id, observer_id, observer_type, pattern, ptype,
         expected_value) {

  /* Raise an event for the DomMutationEventObserver.
   *
   * Args:
   *  None
   */
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
        if (patternMatchesNode(node, pattern, ptype) &&
            nodeValueTextEquals(node, expected_value)) {
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
    if (ptype == "xpath") {
      var node = firstMatchingNode(pattern, ptype);
      if (!node) {
        raiseEvent();
        observer.disconnect();
        delete observer;
      }
    } else {
      for (var j=0; j<mutations.length; j++) {
        for (var i=0; i<mutations[j].removedNodes.length; i++) {
          var node = mutations[j].removedNodes[i];
          if (patternMatchesNode(node, pattern, ptype)) {
            raiseEvent();
            observer.disconnect();
            delete observer;
            return;
          }
        }
      }
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
      if (nodeValueTextEquals(mutations[j].target, expected_value)) {
        raiseEvent();
        observer.disconnect();
        delete observer;
        return;
      }
    }
  }

  /* Return true if the xpath matches the given node.
   *
   * Args:
   *  node: A node object from the DOM.
   *  xpath: A string in the format of an xpath.
   */
  function XPathMatchesNode(node, xpath) {
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
   *  xpath: A string in the format of an xpath.
   */
  function firstXPathNode(xpath) {
    return document.evaluate(xpath, document, null,
        XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;
  }

  /* Returns true if the pattern matches the given node.
   * Args:
   *
   *  node: A node object from the DOM.
   *  pattern: A string in the format of either an XPath or CSS Selector.
   *  ptype: Either 'xpath' or 'css'.
   */
  function patternMatchesNode(node, pattern, ptype) {
    if (ptype == "xpath") {
      return XPathMatchesNode(node, pattern);
    } else if (ptype == "css") {
      return (node.webkitMatchesSelector &&
              node.webkitMatchesSelector(pattern));
    }
    return false;
  }

  /* Returns the first node in the DOM that matches pattern.
   *
   * Args:
   *  pattern: A string in the format of either an XPath or CSS Selector.
   *  ptype: Either 'xpath' or 'css'.
   */
  function firstMatchingNode(pattern, ptype) {
    if (ptype == "xpath") {
      return firstXPathNode(pattern);
    } else if (ptype == "css") {
      return document.querySelector(pattern);
    }
    return null;
  }

  /* Returns true if the node has a textContent attribute equal to
   * expected_value.
   *
   * Args:
   *  node: A node object from the DOM.
   *  expected_value: A regular expression to match with the node's
   *      textContent attribute.
   */
  function nodeValueTextEquals(node, expected_value) {
    return expected_value == null ||
        (node.textContent && RegExp(expected_value, "").test(node.textContent));
  }

  /* Watch for a node matching pattern to be added to the DOM.
   *
   * Args:
   *  pattern: A string in the format of either an XPath or CSS Selector.
   *  ptype: Either 'xpath' or 'css'.
   */
  function observeAdd(pattern, ptype) {
    window.domAutomationController.send("success");
    if (firstMatchingNode(pattern, ptype)) {
      raiseEvent();
      console.log("Matching node in DOM, assuming it was previously added.");
      return;
    }

    var obs = new WebKitMutationObserver(addNodeCallback);
    obs.observe(document,
        { childList: true,
          attributes: true,
          subtree: true});
  }

  /* Watch for a node matching pattern to be removed from the DOM.
   *
   * Args:
   *  pattern: A string in the format of either an XPath or CSS Selector.
   *  ptype: Either 'xpath' or 'css'.
   */
  function observeRemove(pattern, ptype) {
    window.domAutomationController.send("success");
    if (!firstMatchingNode(pattern, ptype)) {
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

  /* Watch for the textContent of a node matching pattern to change to
   * expected_value.
   *
   * Args:
   *  pattern: A string in the format of either an XPath or CSS Selector.
   *  ptype: Either 'xpath' or 'css'.
   */
  function observeChange(pattern, ptype) {
    var node = firstMatchingNode(pattern, ptype);
    if (!node) {
      console.log("No matching node in DOM.");
      window.domAutomationController.send(
          "No DOM node matching pattern exists.");
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


  /* Interpret arguments and launch the requested observer function. */
  var observer;
  switch (observer_type) {
    case "add":
      observeAdd(pattern, ptype);
      break;
    case "remove":
      observeRemove(pattern, ptype);
      break;
    case "change":
      observeChange(pattern, ptype);
      break;
  }

  console.log("MutationObserver javscript injection completed.")

}
