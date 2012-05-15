// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// downloads api test
// browser_tests.exe --gtest_filter=DownloadsApiTest.Downloads

// Comment this out to enable debugging.
console.debug = function() {};

function debugObject(obj) {
  for (var property in obj) {
    console.debug(property + ': ' + obj[property]);
  }
}

var downloads = chrome.experimental.downloads;
window.requestFileSystem  = (window.requestFileSystem ||
                             window.webkitRequestFileSystem);
window.BlobBuilder = (window.BlobBuilder ||
                      window.WebKitBlobBuilder);

chrome.test.getConfig(function(testConfig) {
  function getURL(path) {
    return 'http://localhost:' + testConfig.testServer.port + '/' + path;
  }

  var nextId = 0;
  function getNextId() {
    return nextId++;
  }

  // There can be only one assertThrows per test function.  This should be
  // called as follows:
  //
  //   assertThrows(exceptionMessage, function, [arg1, [arg2, ... ]])
  //
  // ... where |exceptionMessage| is the expected exception message.
  // |arg1|, |arg2|, etc.. are arguments to |function|.
  function assertThrows(exceptionMessage, func) {
    var args = Array.prototype.slice.call(arguments, 2);
    try {
      args.push(function() {
        // Don't use chrome.test.callbackFail because it requires the
        // callback to be called.
        chrome.test.fail('Failed to throw exception (' +
                         exceptionMessage + ')');
      });
      func.apply(null, args);
    } catch (exception) {
      chrome.test.assertEq(exceptionMessage, exception.message);
      chrome.test.succeed();
    }
  }

  // The "/slow" handler waits a specified amount of time before returning a
  // safe file. Specify zero seconds to return quickly.
  var SAFE_FAST_URL = getURL('slow?0');

  var NEVER_FINISH_URL = getURL('download-known-size');

  // This URL should only work with the POST method and a request body
  // containing 'BODY'.
  var POST_URL = getURL('files/post/downloads/a_zip_file.zip?' +
                        'expected_body=BODY');

  // This URL should only work with headers 'Foo: bar' and 'Qx: yo'.
  var HEADERS_URL = getURL('files/downloads/a_zip_file.zip?' +
                           'expected_headers=Foo:bar&expected_headers=Qx:yo');

  // A simple handler that requires http auth basic.
  var AUTH_BASIC_URL = getURL('auth-basic');

  // This is just base64 of 'username:secret'.
  var AUTHORIZATION = 'dXNlcm5hbWU6c2VjcmV0';

  chrome.test.runTests([
    // TODO(benjhayden): Test onErased using remove().

    // TODO(benjhayden): Sub-directories depend on http://crbug.com/109443
    // TODO(benjhayden): Windows slashes.
    // function downloadSubDirectoryFilename() {
    //   var downloadId = getNextId();
    //   var callbackCompleted = chrome.test.callbackAdded();
    //   function myListener(delta) {
    //     if ((delta.id != downloadId) ||
    //         !delta.filename ||
    //         (delta.filename.new.indexOf('/foo/slow') == -1))
    //       return;
    //     downloads.onChanged.removeListener(myListener);
    //     callbackCompleted();
    //   }
    //   downloads.onChanged.addListener(myListener);
    //   downloads.download(
    //       {'url': SAFE_FAST_URL, 'filename': 'foo/slow'},
    //       chrome.test.callback(function(id) {
    //         chrome.test.assertEq(downloadId, id);
    //       }));
    // },

    function downloadBlob() {
      // Test that we can begin a download for a blob.
      var downloadId = getNextId();
      console.debug(downloadId);
      function getBlobURL(data, filename, callback) {
        var dirname = '' + Math.random();
        function fileSystemError(operation, data) {
          return function(fileError) {
            callback(null, {operation: operation,
                            data: data,
                            code: fileError.code});
          }
        }
        window.requestFileSystem(TEMPORARY, 5*1024*1024, function(fs) {
          fs.root.getDirectory(dirname, {create: true, exclusive: true},
                               function(dirEntry) {
            dirEntry.getFile(filename, {create: true, exclusive: true},
                             function(fileEntry) {
              fileEntry.createWriter(function(fileWriter) {
                fileWriter.onwriteend = function(e) {
                  callback(fileEntry.toURL(), null);
                };
                fileWriter.onerror = function(e) {
                  callback(null, ('Write failed: ' + e.toString()));
                };
                var bb = new window.BlobBuilder();
                bb.append(data);
                fileWriter.write(bb.getBlob());
              }, fileSystemError('createWriter'));
            }, fileSystemError('getFile', filename));
          }, fileSystemError('getDirectory',  dirname));
        }, fileSystemError('requestFileSystem'));
      }

      getBlobURL('Lorem ipsum', downloadId + '.txt',
                 chrome.test.callback(function(blobUrl, blobError) {
        if (blobError)
          throw blobError;
        console.debug(blobUrl);
        downloads.download(
            {'url': blobUrl},
            chrome.test.callback(function(id) {
              console.debug(id);
              chrome.test.assertEq(downloadId, id);
            }));
      }));
    },

    function downloadSimple() {
      // Test that we can begin a download.
      var downloadId = getNextId();
      console.debug(downloadId);
      downloads.download(
          {'url': SAFE_FAST_URL},
          chrome.test.callback(function(id) {
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadOnChanged() {
      // Test that download completion is detectable by an onChanged event
      // listener.
      var downloadId = getNextId();
      console.debug(downloadId);
      var callbackCompleted = chrome.test.callbackAdded();
      function myListener(delta) {
        console.debug(delta.id);
        if ((delta.id != downloadId) ||
            !delta.state)
          return;
        chrome.test.assertEq(downloads.STATE_COMPLETE, delta.state.new);
        console.debug(downloadId);
        downloads.onChanged.removeListener(myListener);
        callbackCompleted();
      }
      downloads.onChanged.addListener(myListener);
      downloads.download(
        {"url": SAFE_FAST_URL},
        chrome.test.callback(function(id) {
          console.debug(downloadId);
          chrome.test.assertEq(downloadId, id);
      }));
    },

    function downloadAuthBasicFail() {
      var downloadId = getNextId();
      console.debug(downloadId);

      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides completion.
        if ((delta.id != downloadId) ||
            !delta.state ||
            !delta.error)
          return;
        console.debug(downloadId);
        chrome.test.assertEq(downloads.STATE_INTERRUPTED, delta.state.new);
        chrome.test.assertEq(30, delta.error.new);
        downloads.onChanged.removeListener(changedListener);
        if (changedCompleted) {
          changedCompleted();
          changedCompleted = null;
        }
      }
      downloads.onChanged.addListener(changedListener);

      // Sometimes the DownloadsEventRouter detects the item for the first time
      // after the item has already been interrupted. In this case, the
      // onChanged event never fires, so run the changedListener manually. If
      // the DownloadsEventRouter detects the item before it's interrupted, then
      // the onChanged event should fire correctly.
      var createdCompleted = chrome.test.callbackAdded();
      function createdListener(createdItem) {
        console.debug(createdItem.id);
        // Ignore events for any download besides our own.
        if (createdItem.id != downloadId)
          return;
        console.debug(downloadId);
        downloads.onCreated.removeListener(createdListener);
        createdCompleted();
        if (createdItem.state == downloads.STATE_INTERRUPTED) {
          changedListener({id: downloadId, state: {new: createdItem.state},
                                           error: {new: createdItem.error}});
        }
      }
      downloads.onCreated.addListener(createdListener);

      downloads.download(
          {'url': AUTH_BASIC_URL,
           'filename': downloadId + '.txt'},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadAuthBasicSucceed() {
      var downloadId = getNextId();
      console.debug(downloadId);

      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides completion.
        if ((delta.id != downloadId) ||
            !delta.state)
          return;
        chrome.test.assertEq(downloads.STATE_COMPLETE, delta.state.new);
        console.debug(downloadId);
        downloads.onChanged.removeListener(changedListener);
        changedCompleted();
      }
      downloads.onChanged.addListener(changedListener);

      downloads.download(
          {'url': AUTH_BASIC_URL,
           'headers': [{'name': 'Authorization',
                        'value': 'Basic ' + AUTHORIZATION}],
           'filename': downloadId + '.txt'},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadPostSuccess() {
      // Test the |method| download option.
      var downloadId = getNextId();
      console.debug(downloadId);
      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides completion.
        if ((delta.id != downloadId) ||
            !delta.state)
          return;
        chrome.test.assertEq(downloads.STATE_COMPLETE, delta.state.new);
        console.debug(downloadId);
        downloads.search({id: downloadId},
                          chrome.test.callback(function(items) {
          console.debug(downloadId);
          chrome.test.assertEq(1, items.length);
          chrome.test.assertEq(downloadId, items[0].id);
          debugObject(items[0]);
          var EXPECTED_SIZE = 164;
          chrome.test.assertEq(EXPECTED_SIZE, items[0].bytesReceived);
        }));
        downloads.onChanged.removeListener(changedListener);
        changedCompleted();
      }
      downloads.onChanged.addListener(changedListener);

      downloads.download(
          {'url': POST_URL,
           'method': 'POST',
           'filename': downloadId + '.txt',
           'body': 'BODY'},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadPostWouldFailWithoutMethod() {
      // Test that downloadPostSuccess would fail if the resource requires the
      // POST method, and chrome fails to propagate the |method| parameter back
      // to the server. This tests both that testserver.py does not succeed when
      // it should fail, and this tests how the downloads extension api exposes
      // the failure to extensions.
      var downloadId = getNextId();
      console.debug(downloadId);

      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides interruption.
        if ((delta.id != downloadId) ||
            !delta.state ||
            !delta.error)
          return;
        chrome.test.assertEq(downloads.STATE_INTERRUPTED, delta.state.new);
        chrome.test.assertEq(33, delta.error.new);
        console.debug(downloadId);
        downloads.onChanged.removeListener(changedListener);
        if (changedCompleted) {
          changedCompleted();
          changedCompleted = null;
        }
      }
      downloads.onChanged.addListener(changedListener);

      // Sometimes the DownloadsEventRouter detects the item for the first time
      // after the item has already been interrupted. In this case, the
      // onChanged event never fires, so run the changedListener manually. If
      // the DownloadsEventRouter detects the item before it's interrupted, then
      // the onChanged event should fire correctly.
      var createdCompleted = chrome.test.callbackAdded();
      function createdListener(createdItem) {
        console.debug(createdItem.id);
        // Ignore events for any download besides our own.
        if (createdItem.id != downloadId)
          return;
        console.debug(downloadId);
        downloads.onCreated.removeListener(createdListener);
        createdCompleted();
        if (createdItem.state == downloads.STATE_INTERRUPTED) {
          changedListener({id: downloadId, state: {new: createdItem.state},
                                           error: {new: createdItem.error}});
        }
      }
      downloads.onCreated.addListener(createdListener);

      downloads.download(
          {'url': POST_URL,
           'filename': downloadId + '.txt',  // Prevent 'file' danger.
           'body': 'BODY'},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadPostWouldFailWithoutBody() {
      // Test that downloadPostSuccess would fail if the resource requires the
      // POST method and a request body, and chrome fails to propagate the
      // |body| parameter back to the server. This tests both that testserver.py
      // does not succeed when it should fail, and this tests how the downloads
      // extension api exposes the failure to extensions.
      var downloadId = getNextId();
      console.debug(downloadId);

      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides interruption.
        if ((delta.id != downloadId) ||
            !delta.state ||
            !delta.error)
          return;
        chrome.test.assertEq(downloads.STATE_INTERRUPTED, delta.state.new);
        chrome.test.assertEq(33, delta.error.new);
        if (delta.error) console.debug(delta.error.new);
        console.debug(downloadId);
        downloads.onChanged.removeListener(changedListener);
        if (changedCompleted) {
          changedCompleted();
          changedCompleted = null;
        }
      }
      downloads.onChanged.addListener(changedListener);

      // Sometimes the DownloadsEventRouter detects the item for the first time
      // after the item has already been interrupted. In this case, the
      // onChanged event never fires, so run the changedListener manually. If
      // the DownloadsEventRouter detects the item before it's interrupted, then
      // the onChanged event should fire correctly.
      var createdCompleted = chrome.test.callbackAdded();
      function createdListener(createdItem) {
        console.debug(createdItem.id);
        // Ignore events for any download besides our own.
        if (createdItem.id != downloadId)
          return;
        console.debug(downloadId);
        downloads.onCreated.removeListener(createdListener);
        createdCompleted();
        if (createdItem.state == downloads.STATE_INTERRUPTED) {
          changedListener({id: downloadId, state: {new: createdItem.state},
                                           error: {new: createdItem.error}});
        }
      }
      downloads.onCreated.addListener(createdListener);

      downloads.download(
          {'url': POST_URL,
           'filename': downloadId + '.txt',  // Prevent 'file' danger.
           'method': 'POST'},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadHeadersSuccess() {
      // Test the |header| download option.
      var downloadId = getNextId();
      console.debug(downloadId);
      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides completion.
        if ((delta.id != downloadId) ||
            !delta.state)
          return;
        chrome.test.assertEq(downloads.STATE_COMPLETE, delta.state.new);
        console.debug(downloadId);
        downloads.search({id: downloadId},
                          chrome.test.callback(function(items) {
          console.debug(downloadId);
          chrome.test.assertEq(1, items.length);
          chrome.test.assertEq(downloadId, items[0].id);
          debugObject(items[0]);
          var EXPECTED_SIZE = 164;
          chrome.test.assertEq(EXPECTED_SIZE, items[0].bytesReceived);
        }));
        downloads.onChanged.removeListener(changedListener);
        changedCompleted();
      }
      downloads.onChanged.addListener(changedListener);

      downloads.download(
          {'url': HEADERS_URL,
           'filename': downloadId + '.txt',  // Prevent 'file' danger.
           'headers': [{'name': 'Foo', 'value': 'bar'},
                       {'name': 'Qx', 'value': 'yo'}]},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadHeadersBinarySuccess() {
      // Test the |header| download option.
      var downloadId = getNextId();
      console.debug(downloadId);
      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides completion.
        if ((delta.id != downloadId) ||
            !delta.state)
          return;
        chrome.test.assertEq(downloads.STATE_COMPLETE, delta.state.new);
        console.debug(downloadId);
        downloads.search({id: downloadId},
                          chrome.test.callback(function(items) {
          console.debug(downloadId);
          chrome.test.assertEq(1, items.length);
          chrome.test.assertEq(downloadId, items[0].id);
          debugObject(items[0]);
          var EXPECTED_SIZE = 164;
          chrome.test.assertEq(EXPECTED_SIZE, items[0].bytesReceived);
        }));
        downloads.onChanged.removeListener(changedListener);
        changedCompleted();
      }
      downloads.onChanged.addListener(changedListener);

      downloads.download(
          {'url': HEADERS_URL,
           'filename': downloadId + '.txt',  // Prevent 'file' danger.
           'headers': [{'name': 'Foo', 'binaryValue': [98, 97, 114]},
                       {'name': 'Qx', 'binaryValue': [121, 111]}]},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadHeadersWouldFail() {
      // Test that downloadHeadersSuccess() would fail if the resource requires
      // the headers, and chrome fails to propagate them back to the server.
      // This tests both that testserver.py does not succeed when it should
      // fail as well as how the downloads extension api exposes the
      // failure to extensions.
      var downloadId = getNextId();
      console.debug(downloadId);

      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides interruption.
        if ((delta.id != downloadId) ||
            !delta.state ||
            !delta.error)
          return;
        chrome.test.assertEq(downloads.STATE_INTERRUPTED, delta.state.new);
        chrome.test.assertEq(33, delta.error.new);
        console.debug(downloadId);
        downloads.onChanged.removeListener(changedListener);
        if (changedCompleted) {
          changedCompleted();
          changedCompleted = null;
        }
      }
      downloads.onChanged.addListener(changedListener);

      // Sometimes the DownloadsEventRouter detects the item for the first time
      // after the item has already been interrupted. In this case, the
      // onChanged event never fires, so run the changedListener manually. If
      // the DownloadsEventRouter detects the item before it's interrupted, then
      // the onChanged event should fire correctly.
      var createdCompleted = chrome.test.callbackAdded();
      function createdListener(createdItem) {
        console.debug(createdItem.id);
        // Ignore events for any download besides our own.
        if (createdItem.id != downloadId)
          return;
        console.debug(downloadId);
        downloads.onCreated.removeListener(createdListener);
        createdCompleted();
        if (createdItem.state == downloads.STATE_INTERRUPTED) {
          changedListener({id: downloadId, state: {new: createdItem.state},
                                           error: {new: createdItem.error}});
        }
      }
      downloads.onCreated.addListener(createdListener);

      downloads.download(
          {'url': HEADERS_URL},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadHeadersInvalid0() {
      // Test that we disallow certain headers case-insensitive.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'Accept-chArsEt', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid1() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'accept-eNcoding', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid2() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'coNNection', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid3() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'coNteNt-leNgth', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid4() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'cooKIE', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid5() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'cOOkie2', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid6() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'coNteNt-traNsfer-eNcodiNg', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid7() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'dAtE', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid8() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'ExpEcT', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid9() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'hOsT', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid10() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'kEEp-aLivE', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid11() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'rEfErEr', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid12() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'tE', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid13() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'trAilER', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid14() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'trANsfer-eNcodiNg', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid15() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'upGRAde', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid16() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'usER-agENt', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid17() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'viA', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid18() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'pRoxY-', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid19() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'sEc-', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid20() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'pRoxY-probably-not-evil', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid21() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'sEc-probably-not-evil', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid22() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'oRiGiN', 'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid23() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'Access-Control-Request-Headers',
                        'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadHeadersInvalid24() {
      // Test that we disallow certain headers.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{'name': 'Access-Control-Request-Method',
                        'value': 'evil'}]},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadInterrupted() {
      // Test that cancel()ing an in-progress download causes its state to
      // transition to interrupted, and test that that state transition is
      // detectable by an onChanged event listener.
      // TODO(benjhayden): Test other sources of interruptions such as server
      // death.
      var downloadId = getNextId();
      console.debug(downloadId);

      var createdCompleted = chrome.test.callbackAdded();
      function createdListener(createdItem) {
        console.debug(createdItem.id);
        // Ignore onCreated events for any download besides our own.
        if (createdItem.id != downloadId)
          return;
        console.debug(downloadId);
        // TODO(benjhayden) Move this cancel() into the download() callback
        // after ensuring that DownloadItems are created before that callback
        // is fired.
        downloads.cancel(downloadId, chrome.test.callback(function() {
          console.debug(downloadId);
        }));
        downloads.onCreated.removeListener(createdListener);
        createdCompleted();
      }
      downloads.onCreated.addListener(createdListener);

      var changedCompleted = chrome.test.callbackAdded();
      function changedListener(delta) {
        console.debug(delta.id);
        // Ignore onChanged events for downloads besides our own, or events that
        // signal any change besides interruption.
        if ((delta.id != downloadId) ||
            !delta.state ||
            !delta.error)
          return;
        chrome.test.assertEq(downloads.STATE_INTERRUPTED, delta.state.new);
        chrome.test.assertEq(40, delta.error.new);
        console.debug(downloadId);
        downloads.onChanged.removeListener(changedListener);
        changedCompleted();
      }
      downloads.onChanged.addListener(changedListener);

      downloads.download(
          {'url': NEVER_FINISH_URL},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadFilename() {
      // Test that we can suggest a filename for a new download, and test that
      // we can detect filename changes with an onChanged event listener.
      var FILENAME = 'owiejtoiwjrfoiwjroiwjroiwjroiwjrfi';
      var downloadId = getNextId();
      console.debug(downloadId);
      var callbackCompleted = chrome.test.callbackAdded();
      function myListener(delta) {
        console.debug(delta.id);
        if ((delta.id != downloadId) ||
            !delta.filename ||
            (delta.filename.new.indexOf(FILENAME) == -1))
          return;
        console.debug(downloadId);
        downloads.onChanged.removeListener(myListener);
        callbackCompleted();
      }
      downloads.onChanged.addListener(myListener);
      downloads.download(
          {'url': SAFE_FAST_URL, 'filename': FILENAME},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    // TODO(benjhayden): Update this test when downloading to sub-directories is
    // supported.
    function downloadFilenameDisallowSlashes() {
      downloads.download(
          {'url': SAFE_FAST_URL, 'filename': 'subdirectory/file.txt'},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadOnCreated() {
      // Test that the onCreated event fires when we start a download.
      var downloadId = getNextId();
      console.debug(downloadId);
      var createdCompleted = chrome.test.callbackAdded();
      function createdListener(item) {
        console.debug(item.id);
        if (item.id != downloadId)
          return;
        console.debug(downloadId);
        createdCompleted();
        downloads.onCreated.removeListener(createdListener);
      };
      downloads.onCreated.addListener(createdListener);
      downloads.download(
          {'url': SAFE_FAST_URL},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadInvalidFilename() {
      // Test that we disallow invalid filenames for new downloads.
      downloads.download(
          {'url': SAFE_FAST_URL, 'filename': '../../../../../etc/passwd'},
          chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadEmpty() {
      assertThrows(('Invalid value for argument 1. Property \'url\': ' +
                    'Property is required.'),
                   downloads.download, {});
    },

    function downloadInvalidSaveAs() {
      assertThrows(('Invalid value for argument 1. Property \'saveAs\': ' +
                    'Expected \'boolean\' but got \'string\'.'),
                   downloads.download,
                   {'url': SAFE_FAST_URL, 'saveAs': 'GOAT'});
    },

    function downloadInvalidHeadersOption() {
      assertThrows(('Invalid value for argument 1. Property \'headers\': ' +
                    'Expected \'array\' but got \'string\'.'),
                   downloads.download,
                   {'url': SAFE_FAST_URL, 'headers': 'GOAT'});
    },

    function downloadInvalidURL0() {
      // Test that download() requires a valid url.
      downloads.download(
          {'url': 'foo bar'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL1() {
      // Test that download() requires a valid url, including protocol and
      // hostname.
      downloads.download(
          {'url': '../hello'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL2() {
      // Test that download() requires a valid url, including protocol and
      // hostname.
      downloads.download(
          {'url': '/hello'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL3() {
      // Test that download() requires a valid url, including protocol.
      downloads.download(
          {'url': 'google.com/'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL4() {
      // Test that download() requires a valid url, including protocol and
      // hostname.
      downloads.download(
          {'url': 'http://'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL5() {
      // Test that download() requires a valid url, including protocol and
      // hostname.
      downloads.download(
          {'url': '#frag'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL6() {
      // Test that download() requires a valid url, including protocol and
      // hostname.
      downloads.download(
          {'url': 'foo/bar.html#frag'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadAllowFragments() {
      // Valid URLs plus fragments are still valid URLs.
      var downloadId = getNextId();
      console.debug(downloadId);
      downloads.download(
          {'url': SAFE_FAST_URL + '#frag'},
          chrome.test.callback(function(id) {
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function downloadAllowDataURLs() {
      var downloadId = getNextId();
      downloads.download(
          {'url': 'data:text/plain,hello'},
          chrome.test.callback(function(id) {
              chrome.test.assertEq(downloadId, id);
            }));
    },

    function downloadAllowFileURLs() {
      // Valid file URLs are valid URLs.
      var downloadId = getNextId();
      console.debug(downloadId);
      downloads.download(
          {'url': 'file:///'},
          chrome.test.callback(function(id) {
              chrome.test.assertEq(downloadId, id);
            }));
    },

    function downloadInvalidURL7() {
      // Test that download() rejects javascript urls.
      downloads.download(
          {'url': 'javascript:document.write("hello");'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL8() {
      // Test that download() rejects javascript urls.
      downloads.download(
          {'url': 'javascript:return false;'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    function downloadInvalidURL9() {
      // Test that download() rejects otherwise-valid URLs that fail the host
      // permissions check.
      downloads.download(
          {'url': 'ftp://example.com/example.txt'},
          chrome.test.callbackFail(downloads.ERROR_INVALID_URL));
    },

    // TODO(benjhayden): Set up a test ftp server, add ftp://localhost* to
    // permissions, maybe update downloadInvalidURL9.
    // function downloadAllowFTPURLs() {
    //   // Valid ftp URLs are valid URLs.
    //   var downloadId = getNextId();
    //   console.debug(downloadId);
    //   downloads.download(
    //       {'url': 'ftp://localhost:' + testConfig.testServer.port + '/'},
    //       chrome.test.callback(function(id) {
    //         chrome.test.assertEq(downloadId, id);
    //       }));
    // },

    function downloadInvalidMethod() {
      assertThrows(('Invalid value for argument 1. Property \'method\': ' +
                    'Value must be one of: [GET, POST].'),
                   downloads.download,
                   {'url': SAFE_FAST_URL, 'method': 'GOAT'});
    },

    function downloadInvalidHeader() {
      // Test that download() disallows setting the Cookie header.
      downloads.download(
          {'url': SAFE_FAST_URL,
           'headers': [{ 'name': 'Cookie', 'value': 'fake'}]
        },
        chrome.test.callbackFail(downloads.ERROR_GENERIC));
    },

    function downloadGetFileIconInvalidOptions() {
      assertThrows(('Invalid value for argument 2. Property \'cat\': ' +
                    'Unexpected property.'),
                   downloads.getFileIcon,
                   -1, {cat: 'mouse'});
    },

    function downloadGetFileIconInvalidSize() {
      assertThrows(('Invalid value for argument 2. Property \'size\': ' +
                    'Value must be one of: [16, 32].'),
                   downloads.getFileIcon, -1, {size: 31});
    },

    function downloadGetFileIconInvalidId() {
      downloads.getFileIcon(-42, {size: 32},
        chrome.test.callbackFail(downloads.ERROR_INVALID_OPERATION));
    },

    function downloadPauseInvalidId() {
      downloads.pause(-42, chrome.test.callbackFail(
            downloads.ERROR_INVALID_OPERATION));
    },

    function downloadPauseInvalidType() {
      assertThrows(('Invocation of form experimental.downloads.pause(string,' +
                    ' function) doesn\'t match definition experimental.' +
                    'downloads.pause(integer id, optional function callback)'),
                   downloads.pause,
                   'foo');
    },

    function downloadResumeInvalidId() {
      downloads.resume(-42, chrome.test.callbackFail(
            downloads.ERROR_INVALID_OPERATION));
    },

    function downloadResumeInvalidType() {
      assertThrows(('Invocation of form experimental.downloads.resume(string,' +
                    ' function) doesn\'t match definition experimental.' +
                    'downloads.resume(integer id, optional function callback)'),
                   downloads.resume,
                   'foo');
    },

    function downloadCancelInvalidId() {
      // Canceling a non-existent download is not considered an error.
      downloads.cancel(-42, chrome.test.callback(function() {
        console.debug('');
      }));
    },

    function downloadCancelInvalidType() {
      assertThrows(('Invocation of form experimental.downloads.cancel(string,' +
                    ' function) doesn\'t match definition experimental.' +
                    'downloads.cancel(integer id, optional function callback)'),
                   downloads.cancel, 'foo');
    },

    function downloadNoComplete() {
      // This is used partly to test cleanUp.
      var downloadId = getNextId();
      console.debug(downloadId);
      downloads.download(
          {'url': NEVER_FINISH_URL},
          chrome.test.callback(function(id) {
            console.debug(downloadId);
            chrome.test.assertEq(downloadId, id);
          }));
    },

    function cleanUp() {
      // cleanUp must come last. It clears out all in-progress downloads
      // so the browser can shutdown cleanly.
      console.debug(nextId);
      function makeCallback(id) {
        return function() {
          console.debug(id);
        }
      }
      for (var id = 0; id < nextId; ++id) {
        downloads.cancel(id, chrome.test.callback(makeCallback(id)));
      }
    },

    function callNotifyPass() {
      chrome.test.notifyPass();
      setTimeout(chrome.test.callback(function() {
        console.debug('');
      }), 0);
    }
  ]);
});
