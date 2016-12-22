// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.test.runTests(function() {
  'use strict';

  class StubElement {
    constructor() {
      this.listeners = new Map([
        ['touchstart', []],
        ['touchmove', []],
        ['touchend', []],
        ['touchcancel', []]
      ]);

      this.listenerOptions = new Map();
    }

    addEventListener(type, listener, options) {
      if (this.listeners.has(type)) {
        this.listeners.get(type).push(listener);
        this.listenerOptions.set(listener, options);
      }
    }

    sendEvent(event) {
      for (let l of this.listeners.get(event.type))
        l(event);
    }
  }

  class MockTouchEvent {
    constructor(type, touches) {
      this.type = type;
      this.touches = touches;
      this.defaultPrevented = false;
    }

    preventDefault() {
      this.defaultPrevented = true;
    }
  }

  class PinchListener {
    constructor(gestureDetector) {
      this.lastEvent = null;
      gestureDetector.addEventListener('pinchstart', this.onPinch_.bind(this));
      gestureDetector.addEventListener('pinchupdate', this.onPinch_.bind(this));
      gestureDetector.addEventListener('pinchend', this.onPinch_.bind(this));
    }

    onPinch_(pinchEvent) {
      this.lastEvent = pinchEvent;
    }
  }

  return [
    function testPinchZoomIn() {
      let stubElement = new StubElement();
      let gestureDetector = new GestureDetector(stubElement);
      let pinchListener = new PinchListener(gestureDetector);

      stubElement.sendEvent(new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2}
      ]));
      chrome.test.assertEq({
        type: 'pinchstart',
        center: {x: 0, y: 1}
      }, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 4}
      ]));
      chrome.test.assertEq({
        type: 'pinchupdate',
        scaleRatio: 2,
        direction: 'in',
        startScaleRatio: 2,
        center: {x: 0, y: 2}
      }, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 8}
      ]));
      chrome.test.assertEq({
        type: 'pinchupdate',
        scaleRatio: 2,
        direction: 'in',
        startScaleRatio: 4,
        center: {x: 0, y: 4}
      }, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchend', []));
      chrome.test.assertEq({
        type: 'pinchend',
        startScaleRatio: 4,
        center: {x: 0, y: 4}
      }, pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testPinchZoomInAndBackOut() {
      let stubElement = new StubElement();
      let gestureDetector = new GestureDetector(stubElement);
      let pinchListener = new PinchListener(gestureDetector);

      stubElement.sendEvent(new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2}
      ]));
      chrome.test.assertEq({
        type: 'pinchstart',
        center: {x: 0, y: 1}
      }, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 4}
      ]));
      chrome.test.assertEq({
        type: 'pinchupdate',
        scaleRatio: 2,
        direction: 'in',
        startScaleRatio: 2,
        center: {x: 0, y: 2}
      }, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2}
      ]));
      // This should be part of the same gesture as an update.
      // A change in direction should not end the gesture and start a new one.
      chrome.test.assertEq({
        type: 'pinchupdate',
        scaleRatio: 0.5,
        direction: 'out',
        startScaleRatio: 1,
        center: {x: 0, y: 1}
      }, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchend', []));
      chrome.test.assertEq({
        type: 'pinchend',
        startScaleRatio: 1,
        center: {x: 0, y: 1}
      }, pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testIgnoreTouchScrolling() {
      let stubElement = new StubElement();
      let gestureDetector = new GestureDetector(stubElement);
      let pinchListener = new PinchListener(gestureDetector);

      let touchScrollStartEvent = new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
      ]);
      stubElement.sendEvent(touchScrollStartEvent);
      chrome.test.assertEq(null, pinchListener.lastEvent);
      chrome.test.assertFalse(touchScrollStartEvent.defaultPrevented);

      stubElement.sendEvent(new MockTouchEvent('touchmove', [
        {clientX: 0, clientY: 1},
      ]));
      chrome.test.assertEq(null, pinchListener.lastEvent);

      stubElement.sendEvent(new MockTouchEvent('touchend', []));
      chrome.test.assertEq(null, pinchListener.lastEvent);

      chrome.test.succeed();
    },

    function testPreventNativePinchZoom() {
      let stubElement = new StubElement();
      let gestureDetector = new GestureDetector(stubElement);
      let pinchListener = new PinchListener(gestureDetector);

      // Ensure that the touchstart listener is not passive, otherwise the
      // call to preventDefault will be ignored. Since listeners could default
      // to being passive, we must set the value explicitly
      // (see crbug.com/675730).
      for (let l of stubElement.listeners.get('touchstart')) {
        let options = stubElement.listenerOptions.get(l);
        chrome.test.assertTrue(!!options &&
                               typeof(options.passive) == 'boolean');
        chrome.test.assertFalse(options.passive);
      }

      let pinchStartEvent = new MockTouchEvent('touchstart', [
        {clientX: 0, clientY: 0},
        {clientX: 0, clientY: 2}
      ]);
      stubElement.sendEvent(pinchStartEvent);
      chrome.test.assertEq('pinchstart', pinchListener.lastEvent.type);
      chrome.test.assertTrue(pinchStartEvent.defaultPrevented);

      chrome.test.succeed();
    }
  ];
}());
