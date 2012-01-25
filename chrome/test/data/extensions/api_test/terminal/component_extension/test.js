// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var shellCommand = 'shell\n';
var catCommand = 'cat\n';
var catErrCommand = 'cat 1>&2\n';
var testLine = 'testline\n';

var startCharacter = '#';

var croshName = 'crosh';
var invalidName = 'some name';

var invalidNameError = 'Invalid process name.';

var testLineNum = 10;
var testProcessTotal = 2;

var testProcessCount = 0;
var testProcesses = [];

function TestProcess(pid, type) {
  this.pid_ = pid;
  this.type_= type;
  this.expectation_ = '';
  this.closed_ = false;
  this.startCharactersFound_ = 0;
  this.started_ = false;
};

TestProcess.prototype.testExpectation = function(text) {
  var textPosition = this.expectation_.indexOf(text);
  chrome.test.assertEq(0, textPosition);
  if (this.expectation_.lenght == text.length)
    this.expectation_.clear();
  else
    this.expectation_ = this.expectation_.substring(text.length);
};

TestProcess.prototype.testOutputType = function(receivedType) {
  if (receivedType == 'exit')
    chrome.test.assertEq(this.expectation.length, 0);

  chrome.test.assertEq('stdout', receivedType);
};

TestProcess.prototype.pid = function() {
  return this.pid_;
};

TestProcess.prototype.started = function() {
  return this.started_;
};

TestProcess.prototype.done = function() {
  return this.expectation_.length == 0;
};

TestProcess.prototype.isClosed = function() {
  return this.closed_;
};

TestProcess.prototype.setClosed = function() {
  this.closed_ = true;
};

TestProcess.prototype.canStart = function() {
  return (this.startCharactersFound_ == 2);
};

TestProcess.prototype.startCharacterFound = function() {
  this.startCharactersFound_++;
};

TestProcess.prototype.getCatCommand = function() {
  if (this.type_ == "stdout")
    return catCommand;
  return catErrCommand;
};

TestProcess.prototype.addExpectationLine = function(line) {
  this.expectation_ = this.expectation_.concat(line);
};

// Set of commands we use to setup terminal for testing (start cat) will produce
// some output. We don't care about that output, to avoid having to set that
// output in test expectations, we will send |startCharacter| right after cat is
// started. After we detect second |startCharacter|s in output, we know process
// won't produce any output by itself, so it is safe to start actual test.
TestProcess.prototype.maybeKickOffTest = function(text) {
  var index = 0;
  while (index != -1) {
    index = text.indexOf(startCharacter, index);
    if (index != -1) {
      this.startCharacterFound();
      if (this.canStart()) {
        this.kickOffTest_(testLine, testLineNum);
        return;
      }
      index++;
    }
  }
};

TestProcess.prototype.kickOffTest_ = function(line, lineNum) {
  this.started_ = true;
  // Each line will be echoed twice.
  for (var i = 0; i < lineNum * 2; i++) {
    this.addExpectationLine(line);
  }

  for (var i = 0; i < lineNum; i++)
    chrome.terminalPrivate.sendInput(this.pid_, line,
        function (result) {
          chrome.test.assertTrue(result);
        }
  );
};


function getProcessIndexForPid(pid) {
  for (var i = 0; i < testProcessTotal; i++) {
    if (testProcesses[i] && pid == testProcesses[i].pid())
      return i;
  }
  return undefined;
};

function processOutputListener(pid, type, textReceived) {
  var text = textReceived.replace(/\r/g, '');
  var processIndex = getProcessIndexForPid(pid);
  if (processIndex == undefined)
    return;

  var process = testProcesses[processIndex];

  if (!process.started()) {
    process.maybeKickOffTest(text);
    return;
  }

  process.testOutputType(type);

  process.testExpectation(text);

  if (process.done())
    closeTerminal(processIndex);
};

function maybeEndTest() {
  for (var i = 0; i < testProcessTotal; i++) {
    if (!testProcesses[i] || !testProcesses[i].isClosed())
      return;
  }

  chrome.test.succeed();
};

function closeTerminal(index) {
  var process = testProcesses[index];
  chrome.terminalPrivate.closeTerminalProcess(
      process.pid(),
      function(result) {
        chrome.test.assertTrue(result);
        process.setClosed();
        maybeEndTest();
      }
  );
};

function initTest(process) {
  var sendStartCharacter = function() {
      chrome.terminalPrivate.sendInput(
          process.pid(),
          startCharacter + '\n',
          function(result) {
              chrome.test.assertTrue(result);
          }
      );
  };

  var startCat = function() {
      chrome.terminalPrivate.sendInput(
          process.pid(),
          process.getCatCommand(),
          function(result) {
            chrome.test.assertTrue(result);
            sendStartCharacter();
          }
      );
  };

  chrome.terminalPrivate.sendInput(
      process.pid(),
      shellCommand,
      function(result) {
        chrome.test.assertTrue(result);
        startCat();
      }
  );
};

chrome.test.runTests([
  function terminalTest() {
    chrome.terminalPrivate.onProcessOutput.addListener(processOutputListener);

    for (var i = 0; i < testProcessTotal; i++) {
      chrome.terminalPrivate.openTerminalProcess(croshName, function(result) {
          chrome.test.assertTrue(result >= 0);
          var type = (testProcessCount % 2) ? 'stderr' : 'stdout';
          var newProcess = new TestProcess(result, type);
          testProcesses[testProcessCount] = newProcess;
          testProcessCount++;
          initTest(newProcess);
      });
    }
  },

  function invalidProcessNameTest() {
    chrome.terminalPrivate.openTerminalProcess(invalidName,
        chrome.test.callbackFail(invalidNameError));
  }
]);
