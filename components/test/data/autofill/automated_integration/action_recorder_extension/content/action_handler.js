// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {
  function buildXPathForElement(target) {
    function build(target) {
      let node = target;
      let descendentXPath = '';

      while (node !== null) {
        if (node.nodeType === Node.DOCUMENT_NODE) {
          return descendentXPath;
        }

        let result = prependParentNodeToXPath(node, descendentXPath);
        if (result.isUnique) {
          return result.xPath;
        }

        descendentXPath = result.xPath;
        node = node.parentNode;
      }

      throw('Failed to build a unique XPath! The target node isn\'t ' +
            'attached to the document!');
    }

    function prependParentNodeToXPath(node, xPath) {
      // First, test if simply constructing an XPath using the element's
      // local name results in a unique XPath.
      let nodeXPath = buildXPathForSingleNode(node);
      let testXPath = `//${nodeXPath}${xPath}`;
      if (countNumOfMatches(testXPath) === 1) {
        return { isUnique: true, xPath: testXPath };
      }

      let classifiers = [];
      // Try adding each explicit attribute of the element to the XPath.
      for (let index = 0; index < node.attributes.length; index++) {
        const attr = node.attributes[index].name;
        if (attr === 'style') {
          // Skip styles. 'style' is mutable.
          continue;
        } else if (attr === 'class') {
          // Disabled. Class list is simply too mutable, especially in the
          // case where an element changes class on hover or on focus.
          continue;
        } else if (attr === 'title') {
          // 'title' is an attribute inserted by Chrome Autofill to predict
          // how the field should be filled. Since this attribute is inserted
          // by Chrome, it may change from build to build. Skip this
          // attribute.
          continue;
        } else {
          let classifier = buildClassifier(node, `@${attr}`,
                                           node.attributes[index].value);
          // Add the classifier and see if it generates a unique XPath.
          classifiers.push(classifier);
          nodeXPath = buildXPathForSingleNode(node, classifiers);
          let testXPath = `//${nodeXPath}${xPath}`;
          let numMatches = countNumOfMatches(testXPath);
          if (numMatches === 0) {
            // The classifier is faulty, log an error and remove the
            // classifier.
            console.warn('Encountered faulty classifier: ' +
                         classifiers.pop());
          } else if (numMatches === 1) {
            // The current XPath is unique, exit.
            return { isUnique: true, xPath: testXPath };
          }
        }
      }

      // For HTML elements that contains text content, try to construct
      // an XPath using the text content.
      switch (node.localName) {
        case 'a':
        case 'span':
        case 'button':
          let classifier = buildClassifier(node, 'text()',
                                           node.textContent);
          classifiers.push(classifier);
          nodeXPath = buildXPathForSingleNode(node, classifiers);
          let testXPath = `//${nodeXPath}${xPath}`;
          let numMatches = countNumOfMatches(testXPath);
          if (numMatches === 0) {
            // The classifier is faulty, log an error and remove the
            // classifier.
            console.warn('Encountered faulty classifier: ' +
                         classifiers.pop());
          } else if (numMatches === 1) {
            // The current XPath is unique, exit.
            return { isUnique: true, xPath: testXPath };
          }
          break;
        default:
      }

      // A XPath with the current node as the root is not unique.
      // Check if the node has siblings with the same XPath. If so,
      // add a child node index to the current node's XPath.
      const queryResult = document.evaluate(
          nodeXPath, node.parentNode, null,
          XPathResult.ORDERED_NODE_SNAPSHOT_TYPE, null);

      if (queryResult.snapshotLength === 1) {
        return { isUnique: false, xPath: `/${nodeXPath}${xPath}` };
      } else {
        // The current node has siblings with the same XPath. Add an index
        // to the current node's XPath.
        for (let index = 0; index < queryResult.snapshotLength; index++) {
          if (queryResult.snapshotItem(index) === node) {
            let testXPath = `/${nodeXPath}[${(index + 1)}]${xPath}`;
            return { isUnique: false, xPath: testXPath };
          }
        }

        throw('Assert: unable to find the target node!');
      }
    }

    function buildClassifier(element, attributeXPathToken, attributeValue) {
      // Skip values that has the " character. There is just no good way to
      // wrap this character in xslt, and the best way to handle it involves
      // doing gynmnastics with string concatenation, as in
      // "concat('Single', "'", 'quote. Double', '"', 'quote.')]";
      // It is easier to just skip attributes containing the '"' character.
      // Futhermore, attributes containing the '"' character are very rare.
      if (!attributeValue || attributeValue.indexOf('"') >= 0) {
        return attributeXPathToken;
      }

      // Sometimes, the attribute is a piece of code containing spaces,
      // carriage returns or tabs. A XPath constructed using the raw value
      // will not work. Normalize the value instead.
      const queryResult =
          document.evaluate(`normalize-space(${attributeXPathToken})`, element,
                            null, XPathResult.STRING_TYPE, null);
      if (queryResult.stringValue === attributeValue) {
        // There is no difference between the raw value and the normalized
        // value, return a shorter XPath with the raw value.
        return `${attributeXPathToken}="${attributeValue}"`;
      } else {
        return `normalize-space(${attributeXPathToken})=` +
               `"${queryResult.stringValue}"`;
      }
    }

    function buildXPathForSingleNode(element, classifiers = []) {
      let xPathSelector = element.localName;
      // Add the classifiers
      if (classifiers.length > 0) {
        xPathSelector += `[${classifiers[0]}`;
        for (let index = 1; index < classifiers.length; index++) {
          xPathSelector += ` and ${classifiers[index]}`;
        }
        xPathSelector += ']';
      }
      return xPathSelector;
    }

    function countNumOfMatches(xPath) {
      const queryResult = document.evaluate(
          `count(${xPath})`, document, null, XPathResult.NUMBER_TYPE, null);
      return queryResult.numberValue;
    }

    return build(target);
  };

  let autofillTriggerElementSelector = null;
  let frameContext;
  let mutationObserver = null;
  let started = false;
  let iframeContextMap = {};

  function isAutofillableElement(element) {
     const autofillPrediction = element.getAttribute('autofill-prediction');
    return (autofillPrediction !== null && autofillPrediction !== '' &&
            autofillPrediction !== 'UNKNOWN_TYPE');
  }

  function isEditableInputElement(element) {
    return (element.localName === 'select') ||
           (element.localName === 'textarea') ||
           canTriggerAutofill(element);
  }

  function canTriggerAutofill(element) {
    return (element.localName === 'input' &&
            ['checkbox', 'radio', 'button', 'submit', 'hidden', 'reset']
            .indexOf(element.getAttribute('type')) === -1);
  }

  /**
   * Returns true if |element| is probably a clickable element.
   *
   * @param  {Element} element The element to be checked.
   * @return {boolean}         True if the element is probably clickable.
   */
  function isClickableElementOrInput(element) {
    return (element.tagName == 'A' ||
            element.tagName == 'BUTTON' ||
            element.tagName == 'IMG' ||
            element.tagName == 'INPUT' ||
            element.tagName == 'LABEL' ||
            element.tagName == 'SPAN' ||
            element.tagName == 'SUBMIT' ||
            element.getAttribute('href'));
  }

  function addActionToRecipe(action) {
    return sendRuntimeMessageToBackgroundScript({
      type: RecorderMsgEnum.ADD_ACTION,
      action: action
    });
  }

  function onInputChangeActionHandler(event) {
    const selector = buildXPathForElement(event.target);
    if (isAutofillableElement(event.target)) {
      const autofillPrediction =
          event.target.getAttribute('autofill-prediction');
      // If a field that can trigger autofill has previously been
      // clicked, fire a trigger autofill event.
      if (autofillTriggerElementSelector !== null) {
        console.log(`Triggered autofill on ${autofillTriggerElementSelector}`);
        addActionToRecipe({
          selector: selector,
          context: frameContext,
          type: 'autofill'
        });
        autofillTriggerElementSelector = null;
      }
      addActionToRecipe({
        selector: selector,
        context: frameContext,
        expectedAutofillType: autofillPrediction,
        expectedValue: event.target.value,
        type: 'validateField'
      });
    } else if (event.target.localName === 'select') {
      const index = event.target.options.selectedIndex;
      console.log(`Select detected on: ${selector} with '${index}'`);
      addActionToRecipe({
        selector: selector,
        context: frameContext,
        index: index,
        type: 'select'
      });
    } else {
      console.log(`Typing detected on: ${selector}`);
      addActionToRecipe({
        selector: selector,
        context: frameContext,
        value: event.target.value,
        type: 'type'
      });
    }
  }

  function registerOnInputChangeActionListener(root) {
    const inputElements = root.querySelectorAll('input, select, textarea');
    inputElements.forEach((element) => {
      if (isEditableInputElement(element)) {
        element.addEventListener('change', onInputChangeActionHandler, true);
      }
    });
  }

  function deRegisterOnInputChangeActionListener(root) {
    const inputElements = root.querySelectorAll('input, select, textarea');
    inputElements.forEach((element) => {
      if (isEditableInputElement(element)) {
        element.removeEventListener('change', onInputChangeActionHandler, true);
      }
    });
  }

  function onClickActionHander(event) {
    if (event.button === Buttons.LEFT_BUTTON &&
        // Ignore if the event target is the html element. The click event
        // triggers with the entire html element as the target when the user
        // clicks on a scroll bar.
        event.target.localName !== 'html') {
      const element = event.target;
      const selector = buildXPathForElement(element);
      console.log(`Left-click detected on: ${selector}`);

      if (isEditableInputElement(element)) {
        // If a field that can trigger autofill is clicked, save the
        // the element selector path, as the user could have clicked
        // this element to trigger autofill.
        if (isAutofillableElement(element) && canTriggerAutofill(element)) {
          autofillTriggerElementSelector = selector;
        }
      } else {
        addActionToRecipe({
          selector: selector,
          context: frameContext,
          type: 'click'
        });
        autofillTriggerElementSelector = null;
      }
    } else if (event.button === Buttons.RIGHT_BUTTON) {
      const element = event.target;
      const selector = buildXPathForElement(element);
      console.log(`Right-click detected on: ${selector}`);
      addActionToRecipe({
          selector: selector,
          context: frameContext,
          type: 'hover'
        });
    }
  }

  function startRecording() {
    const promise =
      // First, obtain the current frame's context.
      sendRuntimeMessageToBackgroundScript({
          type: RecorderMsgEnum.GET_FRAME_CONTEXT,
          location: location})
      .then((context) => {
        frameContext = context;
        // Register on change listeners on all the input elements.
        registerOnInputChangeActionListener(document);
        // Register a mouse up listener on the entire dom.
        //
        // The content script registers a 'Mouse Up' listener rather than a
        // 'Mouse Down' to correctly handle the following scenario:
        //
        // A user types inside a search box, then clicks the search button.
        //
        // The following events will fire in quick chronological succession:
        // * Mouse down on the search button.
        // * Change on the search input box.
        // * Mouse up on the search button.
        //
        // To capture the correct sequence of actions, the content script
        // should tie left mouse click actions to the mouseup event.
        document.addEventListener('mouseup', onClickActionHander);
        // Setup mutation observer to listen for event on nodes added after
        // recording starts.
        mutationObserver = new MutationObserver((mutations) => {
          mutations.forEach(mutation => {
            mutation.addedNodes.forEach(node => {
              if (node.nodeType === Node.ELEMENT_NODE) {
                // Add the onchange listener on any new input elements. This
                // way the recorder can record user interactions with new
                // elements.
                registerOnInputChangeActionListener(node);
              }
            });
          });
        });
        mutationObserver.observe(document, {childList: true, subtree: true});
        started = true;
        return Promise.resolve();
      });
    return promise;
  }

  function stopRecording() {
    if (started) {
      mutationObserver.disconnect();
      document.removeEventListener('mousedown', onClickActionHander);
      deRegisterOnInputChangeActionListener(document);
    }
  }

  function queryIframeContext(frameId, frameLocation) {
    // Check if we cached the XPath for this iframe.
    if (iframeContextMap[frameId]) {
      return Promise.resolve(iframeContextMap[frameId]);
    }

    let iframe = null;
    const iframes = document.querySelectorAll('iframe');
    let numIframeWithSameSchemeAndHost = 0;
    // Find the target iframe.
    for (let index = 0; index < iframes.length; index++) {
      const url = new URL(iframes[index].src,
                          `${location.protocol}//${location.host}`);
      // Try to identify the iframe using the entire URL.
      if (frameLocation.href === url.href) {
        iframe = iframes[index];
      }
      // On rare occasions, the hash in an iframe's URL may not match with the
      // hash in the iframe element's src attribute. For example, the iframe
      // document reports that the iframe url is 'http://foobar.com/index#bar',
      // whereas in the parent document, the iframe element's src attribute is
      // '/index'
      // To handle the scenario described above, this code optionally ignores
      // the iframe url's hash.
      if (iframes === null &&
          frameLocation.protocol === url.protocol &&
          frameLocation.host === url.host &&
          frameLocation.pathname === url.pathname &&
          frameLocation.search === url.search) {
        iframe = iframes[index];
      }

      // Count the number of iframes with the same protocol and the same host.
      if (frameLocation.protocol === url.protocol &&
          frameLocation.host === url.host) {
        numIframeWithSameSchemeAndHost++;
      }
    }

    if (iframe === null) {
      return Promise.reject(
          new Error('Unable to find iframe with url ' +
                    `'${frameLocation.href}', for frame ${frameId}`));
    }

    let context = { isIframe: true, browser_test: {} };
    if (iframe.name) {
      context.browser_test.name = iframe.name;
    }
    else if (numIframeWithSameSchemeAndHost === 1) {
      // Register the iframe's scheme, host and port.
      // The Captured Site automation framework can identify an iframe by its
      // scheme + host + port, provided this information combination is unique.
      // Identifying an iframe through its scheme + host + port is preferable
      // than identityfing an iframe through its URL. An URL will frequently
      // contain parameters, and many websites use random number generator
      // or date generator to create these parameters. For example, in the
      // following URL
      //
      // https://payment.bhphotovideo.com/static/desktop/v2.0/index.html
      // #paypageId=aLGNuLSTJVwgEiCn&cartID=333334444
      // &receiverID=77777777-7777-4777-b777-777777888888
      // &uuid=77777777-7777-4777-b777-778888888888
      //
      // The site created the parameters cartID, receiverID and uuid using
      // random number generators. These parameters will have different values
      // every time the browser loads the page. Therefore automation will not
      // be able to identify an iframe that loads this URL.
      context.browser_test.schemeAndHost =
          `${frameLocation.protocol}//${frameLocation.host}`;
    } else {
      context.browser_test.url = frameLocation.href;
    }

    iframeContextMap[frameId] = context;
    return Promise.resolve(context);
  }

  chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
    if (!request) return;
    switch (request.type) {
      case RecorderMsgEnum.START:
        startRecording()
        .then(() => sendResponse(true))
        .catch((error) => {
          sendResponse(false);
          console.error(
              `Unable to start recording on ${window.location.href }!\r\n`,
              error);
        });
        return true;
      case RecorderMsgEnum.STOP:
        stopRecording();
        sendResponse(true);
        break;
      case RecorderMsgEnum.GET_IFRAME_XPATH:
        queryIframeContext(request.frameId, request.location)
        .then((context) => {
          sendResponse(context);
        }).catch((error) => {
          sendResponse(false);
          console.error(`Unable to query for iframe!\r\n`, error);
        });
        return true;
      default:
    }
    // Return false to close the message channel. Lingering message
    // channels will prevent background scripts from unloading, causing it
    // to persist.
    return false;
  });
})();
