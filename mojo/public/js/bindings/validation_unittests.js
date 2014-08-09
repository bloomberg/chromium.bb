// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
    "file",
    "gin/test/expect",
    "mojo/public/interfaces/bindings/tests/validation_test_interfaces.mojom",
    "mojo/public/js/bindings/buffer",
    "mojo/public/js/bindings/codec",
    "mojo/public/js/bindings/tests/validation_test_input_parser",
    "mojo/public/js/bindings/validator",
  ], function(file, expect, testInterface, buffer, codec, parser, validator) {

  function checkTestMessageParser() {
    function TestMessageParserFailure(message, input) {
      this.message = message;
      this.input = input;
    }

    TestMessageParserFailure.prototype.toString = function() {
      return 'Error: ' + this.message + ' for "' + this.input + '"';
    }

    function checkData(data, expectedData, input) {
      if (data.byteLength != expectedData.byteLength) {
        var s = "message length (" + data.byteLength + ") doesn't match " +
            "expected length: " + expectedData.byteLength;
        throw new TestMessageParserFailure(s, input);
      }

      for (var i = 0; i < data.byteLength; i++) {
        if (data.getUint8(i) != expectedData.getUint8(i)) {
          var s = 'message data mismatch at byte offset ' + i;
          throw new TestMessageParserFailure(s, input);
        }
      }
    }

    function testFloatItems() {
      var input = '[f]+.3e9 [d]-10.03';
      var msg = parser.parseTestMessage(input);
      var expectedData = new buffer.Buffer(12);
      expectedData.setFloat32(0, +.3e9);
      expectedData.setFloat64(4, -10.03);
      checkData(msg.buffer, expectedData, input);
    }

    function testUnsignedIntegerItems() {
      var input = '[u1]0x10// hello world !! \n\r  \t [u2]65535 \n' +
          '[u4]65536 [u8]0xFFFFFFFFFFFFF 0 0Xff';
      var msg = parser.parseTestMessage(input);
      var expectedData = new buffer.Buffer(17);
      expectedData.setUint8(0, 0x10);
      expectedData.setUint16(1, 65535);
      expectedData.setUint32(3, 65536);
      expectedData.setUint64(7, 0xFFFFFFFFFFFFF);
      expectedData.setUint8(15, 0);
      expectedData.setUint8(16, 0xff);
      checkData(msg.buffer, expectedData, input);
    }

    function testSignedIntegerItems() {
      var input = '[s8]-0x800 [s1]-128\t[s2]+0 [s4]-40';
      var msg = parser.parseTestMessage(input);
      var expectedData = new buffer.Buffer(15);
      expectedData.setInt64(0, -0x800);
      expectedData.setInt8(8, -128);
      expectedData.setInt16(9, 0);
      expectedData.setInt32(11, -40);
      checkData(msg.buffer, expectedData, input);
    }

    function testByteItems() {
      var input = '[b]00001011 [b]10000000  // hello world\n [b]00000000';
      var msg = parser.parseTestMessage(input);
      var expectedData = new buffer.Buffer(3);
      expectedData.setUint8(0, 11);
      expectedData.setUint8(1, 128);
      expectedData.setUint8(2, 0);
      checkData(msg.buffer, expectedData, input);
    }

    function testAnchors() {
      var input = '[dist4]foo 0 [dist8]bar 0 [anchr]foo [anchr]bar';
      var msg = parser.parseTestMessage(input);
      var expectedData = new buffer.Buffer(14);
      expectedData.setUint32(0, 14);
      expectedData.setUint8(4, 0);
      expectedData.setUint64(5, 9);
      expectedData.setUint8(13, 0);
      checkData(msg.buffer, expectedData, input);
    }

    function testHandles() {
      var input = '// This message has handles! \n[handles]50 [u8]2';
      var msg = parser.parseTestMessage(input);
      var expectedData = new buffer.Buffer(8);
      expectedData.setUint64(0, 2);

      if (msg.handleCount != 50) {
        var s = 'wrong handle count (' + msg.handleCount + ')';
        throw new TestMessageParserFailure(s, input);
      }
      checkData(msg.buffer, expectedData, input);
    }

    function testEmptyInput() {
      var msg = parser.parseTestMessage('');
      if (msg.buffer.byteLength != 0)
        throw new TestMessageParserFailure('expected empty message', '');
    }

    function testBlankInput() {
      var input = '    \t  // hello world \n\r \t// the answer is 42   ';
      var msg = parser.parseTestMessage(input);
      if (msg.buffer.byteLength != 0)
        throw new TestMessageParserFailure('expected empty message', input);
    }

    function testInvalidInput() {
      function parserShouldFail(input) {
        try {
          parser.parseTestMessage(input);
        } catch (e) {
          if (e instanceof parser.InputError)
            return;
          throw new TestMessageParserFailure(
            'unexpected exception ' + e.toString(), input);
        }
        throw new TestMessageParserFailure("didn't detect invalid input", file);
      }

      ['/ hello world',
       '[u1]x',
       '[u2]-1000',
       '[u1]0x100',
       '[s2]-0x8001',
       '[b]1',
       '[b]1111111k',
       '[dist4]unmatched',
       '[anchr]hello [dist8]hello',
       '[dist4]a [dist4]a [anchr]a',
       // '[dist4]a [anchr]a [dist4]a [anchr]a',
       '0 [handles]50'
      ].forEach(parserShouldFail);
    }

    try {
      testFloatItems();
      testUnsignedIntegerItems();
      testSignedIntegerItems();
      testByteItems();
      testInvalidInput();
      testEmptyInput();
      testBlankInput();
      testHandles();
      testAnchors();
    } catch (e) {
      return e.toString();
    }
    return null;
  }

  function getMessageTestFiles() {
    var sourceRoot = file.getSourceRootDirectory();
    expect(sourceRoot).not.toBeNull();

    var testDir = sourceRoot +
      "/mojo/public/interfaces/bindings/tests/data/validation/";
    var testFiles = file.getFilesInDirectory(testDir);
    expect(testFiles).not.toBeNull();
    expect(testFiles.length).toBeGreaterThan(0);

   // The ".data" pathnames with the extension removed.
   var testPathNames = testFiles.filter(function(s) {
     return s.substr(-5) == ".data";
   }).map(function(s) {
     return testDir + s.slice(0, -5);
   });

   // For now, just checking the message header tests.
   return testPathNames.filter(function(s) {
     return s.indexOf("_msghdr_") != -1;
   });
  }

  function readTestMessage(filename) {
    var contents = file.readFileToString(filename + ".data");
    expect(contents).not.toBeNull();
    return parser.parseTestMessage(contents);
  }

  function readTestExpected(filename) {
    var contents = file.readFileToString(filename + ".expected");
    expect(contents).not.toBeNull();
    return contents.trim();
  }

  function testValidateMessageHeader() {
    var testFiles = getMessageTestFiles();
    expect(testFiles.length).toBeGreaterThan(0);

    for (var i = 0; i < testFiles.length; i++) {
      var testMessage = readTestMessage(testFiles[i]);
      // TODO(hansmuller): add the message handles.
      var message = new codec.Message(testMessage.buffer);
      var actualResult = new validator.Validator(message).validateMessage();
      var expectedResult = readTestExpected(testFiles[i]);
      expect(actualResult).toEqual(expectedResult);
    }
  }

  testValidateMessageHeader();
  expect(checkTestMessageParser()).toBeNull();
  this.result = "PASS";
});
