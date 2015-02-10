// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Unit tests for the JS stash client.
 *
 * They are launched by extensions/renderer/mojo/stash_client_unittest.cc
 */

var test = require('test').binding;
var unittestBindings = require('test_environment_specific_bindings');
var utils = require('utils');

var modulesPromise = Promise.all([
  requireAsync('stash_client'),
  requireAsync('extensions/common/mojo/stash.mojom'),
  requireAsync('mojo/public/js/core'),
]);

function writeAndClose(core, handle) {
  var data = new Int8Array(1);
  data[0] = 'a'.charCodeAt(0);
  var result = core.writeData(handle, data, core.WRITE_DATA_FLAG_NONE);
  test.assertEq(core.RESULT_OK, result.result);
  test.assertEq(1, result.numBytes);
  core.close(handle);
}

function readAndClose(core, handle) {
  var result = core.readData(handle, core.READ_DATA_FLAG_NONE);
  test.assertEq(core.RESULT_OK, result.result);
  core.close(handle);
  test.assertEq(1, result.buffer.byteLength);
  test.assertEq('a'.charCodeAt(0), new Int8Array(result.buffer)[0]);
}

unittestBindings.exportTests([
  function testStash() {
    modulesPromise.then(function(modules) {
      var stashClient = modules[0];
      var stashMojom = modules[1];
      var core = modules[2];
      stashClient.registerClient('test', stashMojom.StashedObject, function() {
        var stashedObject = new stashMojom.StashedObject();
        stashedObject.id = 'test id';
        stashedObject.data = [1, 2, 3, 4];
        var dataPipe = core.createDataPipe({
          flags: core.CREATE_DATA_PIPE_OPTIONS_FLAG_NONE,
          elementNumBytes: 1,
          capacityNumBytes: 10,
        });
        writeAndClose(core, dataPipe.producerHandle);
        stashedObject.stashed_handles = [dataPipe.consumerHandle];
        return [{serialization: stashedObject, monitorHandles: true}];
      });
    }).then(test.succeed, test.fail);
  },

  function testRetrieve() {
    modulesPromise.then(function(modules) {
      var stashClient = modules[0];
      var stashMojom = modules[1];
      var core = modules[2];
      return stashClient.retrieve('test', stashMojom.StashedObject)
          .then(function(stashedObjects) {
        test.assertEq(1, stashedObjects.length);
        test.assertEq('test id', stashedObjects[0].id);
        test.assertEq([1, 2, 3, 4], stashedObjects[0].data);
        test.assertEq(1, stashedObjects[0].stashed_handles.length);
        readAndClose(core, stashedObjects[0].stashed_handles[0]);
      });
    }).then(test.succeed, test.fail);
  },

], test.runTests, exports);
