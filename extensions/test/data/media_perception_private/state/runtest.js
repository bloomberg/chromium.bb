// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function getStateUninitialized() {
  chrome.mediaPerceptionPrivate.getState(
      chrome.test.callbackPass(function(state) {
        chrome.test.assertEq({ status: 'UNINITIALIZED' }, state);
      }));
}

function setStateRunning() {
  chrome.mediaPerceptionPrivate.setState({
    status: 'RUNNING',
    deviceContext: 'device_context'
  }, chrome.test.callbackPass(function(state) {
    chrome.test.assertEq('RUNNING', state.status);
  }));
}

function getStateRunning() {
  chrome.mediaPerceptionPrivate.getState(
      chrome.test.callbackPass(function(state) {
        chrome.test.assertEq('RUNNING', state.status);
      }));
}

function setStateUnsettable() {
  const error = 'Status can only be set to RUNNING and SUSPENDED.';
  chrome.mediaPerceptionPrivate.setState({
    status: 'UNINITIALIZED'
  }, chrome.test.callbackFail(error));
  chrome.mediaPerceptionPrivate.setState({
    status: 'STARTED'
  }, chrome.test.callbackFail(error));
}

function setStateSuspendedButWithDeviceContextFail() {
  const error = 'Only provide deviceContext with SetState RUNNING.';
  chrome.mediaPerceptionPrivate.setState({
    status: 'SUSPENDED',
    deviceContext: 'device_context'
  }, chrome.test.callbackFail(error));
}

chrome.test.runTests([
    getStateUninitialized,
    setStateRunning,
    getStateRunning,
    setStateUnsettable,
    setStateSuspendedButWithDeviceContextFail]);

