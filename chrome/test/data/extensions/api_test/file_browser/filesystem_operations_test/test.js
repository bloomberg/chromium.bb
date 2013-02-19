// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Component extension that tests the extensions with fileBrowesrPrivate
 * permission are able to perform file system operations on external file
 * systems. The test can be run for three different external file system types:
 * local native, restricted local native and drive. The exact file system for
 * which the test is run is determined by trying to get the file systems root
 * dir ('local', 'restricted' or 'drive'). C++ side of the test
 * (external_file_system_extension_apitest.cc) should make sure the file system
 * root dirs are properly named and that exactly one of them exists.
 *
 * The test files on file systems should be created before running the test
 * extension. The test extension expects following hierarchy:
 *   mount_point_root - test_dir - - subdir
 *                                |
 *                                 - empty_test_dir
 *                                |
 *                                 - test_file.xul
 *                                |
 *                                 - test_file.xul.foo
 *                                |
 *                                 - test_file.tiff
 *                                |
 *                                 - test_file.tiff.foo
 *
 * mount_point_root/test_dir/subdir/ will be used as destination dir for copy
 * and move operations.
 * mount_point_root/test_dir/empty_test_dir/ should be empty and will stay empty
 * until the end of the test.
 * mount_point_root/test_dir/test_file.xul will not change during the test.
 *
 * All files should initially have content: kInitialFileContent.
 */

var kInitialFileContent = 'xxxxxxxxxxxxx';
var kWriteOffset = 13;
var kWriteData = '!!!';
var kFileContentAfterWrite = 'xxxxxxxxxxxxx!!!';
var kTruncateShortLength = 5;
var kFileContentAfterTruncateShort = 'xxxxx';
var kTruncateLongLength = 7;
var kFileContentAfterTruncateLong = 'xxxxx\0\0';

function assertEqAndRunCallback(expectedValue, value, errorMessage,
                                callback, callbackArg) {
  chrome.test.assertEq(expectedValue, value, errorMessage);

  callback(callbackArg);
}

/**
 * Helper methods for performing file system operations during tests.
 * Each of them will call |callback| on success, or fail the current test
 * otherwise.
 *
 * callback for |getDirectory| and |getFile| functions expects the gotten entry
 * as an argument. For Other methods, the callback argument should be ignored.
 */

// Gets the directory mountPoint/path
function getDirectory(mountPoint, path, shouldCreate, expectSuccess, callback) {
  var messagePrefix = shouldCreate ? 'Creating ' : 'Getting ';
  var message = messagePrefix + 'directory: \'' + path +'\'.';

  mountPoint.getDirectory(
      path, {create: shouldCreate},
      assertEqAndRunCallback.bind(null, expectSuccess, true, message, callback),
      assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                  callback, null));
}

// Gets the file mountPoint/path
function getFile(mountPoint, path, shouldCreate, expectSuccess, callback) {
  var messagePrefix = shouldCreate ? 'Creating ' : 'Getting ';
  var message = messagePrefix + 'file: \'' + path +'\'.';

  mountPoint.getFile(
      path, {create: shouldCreate},
      assertEqAndRunCallback.bind(null, expectSuccess, true, message, callback),
      assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                  callback, null));
}

// Reads file mountPoint/path and verifies its content. The read operation
// should always succeed.
function readFileAndExpectContent(mountPoint, path, expectedContent, callback) {
  var message = 'Content of the file \'' + path + '\'.';

  getFile(mountPoint, path, false, true, function(entry) {
    var reader = new FileReader();
    reader.onload = function() {
      assertEqAndRunCallback(expectedContent, reader.result, message, callback);
    };
    reader.onerror = chrome.test.fail.bind(null, 'Reading file.');

    entry.file(reader.readAsText.bind(reader),
               chrome.test.fail.bind(null, 'Getting file.'));
  });
}

// Writes |content| to the file mountPoint/path  with offest |offest|.
function writeFile(mountPoint, path, offset, content, expectSuccess, callback) {
  var message = 'Writing to file: \'' + path + '\'.';

  getFile(mountPoint, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      writer.onwrite = assertEqAndRunCallback.bind(null,
          expectSuccess, true, message, callback);
      writer.onerror = assertEqAndRunCallback.bind(null,
          expectSuccess, false, message, callback);

      writer.seek(offset);
      writer.write(new Blob([content], {'type': 'text/plain'}));
    },
    assertEqAndRunCallback.bind(null, expectSuccess, false,
        'Creating writer for \'' + path + '\'.', callback));
  });
}

// Starts and aborts write operation to mountPoint/path.
function abortWriteFile(mountPoint, path, callback) {
  getFile(mountPoint, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      var aborted = false;
      var failed = false;

      writer.onwritestart = function() { writer.abort(); };
      writer.onwrite = function() { failed = true; };
      writer.onerror = function() { failed = true; };
      writer.onabort = function() { aborted = true; };

      writer.onwriteend = function() {
        chrome.test.assertTrue(aborted);
        chrome.test.assertFalse(failed);
        callback();
      }

      writer.write(new Blob(['xxxxx'], {'type': 'text/plain'}));
    },
    chrome.test.fail.bind(null, 'Error creating writer.'));
  });
}

// Truncates file mountPoint/path to lenght |lenght|.
function truncateFile(mountPoint, path, length, expectSuccess, callback) {
  var message = 'Truncating file: \'' + path + '\' to length ' + length + '.';

  getFile(mountPoint, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      writer.onwrite = assertEqAndRunCallback.bind(null,
          expectSuccess, true, message, callback);
      writer.onerror = assertEqAndRunCallback.bind(null,
          expectSuccess, false, message, callback);

      writer.truncate(length);
    },
    assertEqAndRunCallback.bind(null, expectSuccess, false,
        'Creating writer for \'' + path + '\'.', callback));
  });
}

// Starts and aborts truncate operation on mountPoint/path.
function abortTruncateFile(mountPoint, path, callback) {
  getFile(mountPoint, path, false, true, function(entry) {
    entry.createWriter(function(writer) {
      var aborted = false;
      var failed = false;

      writer.onwritestart = function() { writer.abort(); };
      writer.onwrite = function() { failed = true; };
      writer.onerror = function() { failed = true; };
      writer.onabort = function() { aborted = true; };

      writer.onwriteend = function() {
        chrome.test.assertTrue(aborted);
        chrome.test.assertFalse(failed);
        callback();
      }

      writer.truncate(10);
    },
    chrome.test.fail.bind(null, 'Error creating writer.'));
  });
}

// Copies file mountPoint/from to mountPoint/to/newName.
function copyFile(mountPoint, from, to, newName, expectSuccess, callback) {
  var message = 'Copying \'' + from + '\' to \'' + to + '/' + newName + '\'.';

  getFile(mountPoint, from, false, true, function(sourceEntry) {
    getDirectory(mountPoint, to, false, true, function(targetDir) {
      sourceEntry.copyTo(targetDir, newName,
          assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                      callback),
          assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                      callback));
    });
  });
}

// Moves file mountPoint/from to mountPoint/to/newName.
function moveFile(mountPoint, from, to, newName, expectSuccess, callback) {
  var message = 'Moving \'' + from + '\' to \'' + to + '/' + newName + '\'.';

  getFile(mountPoint, from, false, true, function(sourceEntry) {
    getDirectory(mountPoint, to, false, true, function(targetDir) {
      sourceEntry.moveTo(targetDir, newName,
          assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                      callback),
          assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                      callback));
    });
  });
}

// Deletes file mountPoint/path.
function deleteFile(mountPoint, path, expectSuccess, callback) {
  var message = 'Deleting file \'' + path + '\'.';

  getFile(mountPoint, path, false, true, function(entry) {
    entry.remove(
        assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                    callback),
        assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                    callback));
  });
}

// Deletes directory mountPoint/path.
function deleteDirectory(mountPoint, path, expectSuccess, callback) {
  var message = 'Deleting directory \'' + path + '\'.';

  getDirectory(mountPoint, path, false, true, function(entry) {
    entry.remove(
        assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                    callback),
        assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                    callback));
  });
}

// Recursively deletes directory mountPoint/path.
function deleteDirectoryRecursively(mountPoint, path, expectSuccess, callback) {
  var message = 'Recursively deleting directory \'' + path + '\'.';

  getDirectory(mountPoint, path, false, true, function(entry) {
    entry.removeRecursively(
        assertEqAndRunCallback.bind(null, expectSuccess, true, message,
                                    callback),
        assertEqAndRunCallback.bind(null, expectSuccess, false, message,
                                    callback));
  });
}

/**
 * Collects all tests that should be run for the test mount point.
 *
 * @param {string} mountPointName The name of the mount points root directory.
 * @param {DirectoryEntry} mountPoint The mount point's root directory.
 * @returns {Array.<function()>} The list of tests that should be run.
 */
function collectTestsForMountPoint(mountPointName, mountPoint) {
  var isReadOnly = mountPointName == 'restricted/';
  var isOnDrive = mountPointName == 'drive/';

  var testsToRun = [];

  testsToRun.push(function getDirectoryTest() {
    getDirectory(mountPoint, 'test_dir', false, true, chrome.test.succeed);
  });

  testsToRun.push(function createDirectoryTest() {
    // callback checks whether the new directory exists after create operation.
    // It should exists iff the file system is not read only.
    var callback =
        getDirectory.bind(null, mountPoint, 'new_test_dir', false, !isReadOnly,
                          chrome.test.succeed);

    // Create operation should succeed only for non read-only file systems.
    getDirectory(mountPoint, 'new_test_dir', true, !isReadOnly, callback);
  });

  testsToRun.push(function getFileTest() {
    getFile(mountPoint, 'test_dir/test_file.xul', false, true,
            chrome.test.succeed);
  });

  testsToRun.push(function createFileTest() {
    // Checks whether the new file exists after create operation.
    // It should exists iff the file system is not read only.
    var callback = getFile.bind(null, mountPoint, 'test_dir/new_file', false,
        !isReadOnly, chrome.test.succeed);

    // Create operation should succeed only for non read-only file systems.
    getFile(mountPoint, 'test_dir/new_file', true, !isReadOnly, callback);
  });

  testsToRun.push(function readFileTest() {
    readFileAndExpectContent(mountPoint, 'test_dir/test_file.xul',
        kInitialFileContent, chrome.test.succeed);
  });

  testsToRun.push(function writeFileTest() {
    var expectedFinalContent = isReadOnly ? kInitialFileContent :
                                            kFileContentAfterWrite;
    // Check file content after write operation. The content should not change
    // on read-only file system.
    var callback = readFileAndExpectContent.bind(null,
        mountPoint, 'test_dir/test_file.tiff', expectedFinalContent,
        chrome.test.succeed);

    // Write should fail only on read-only file system.
    writeFile(mountPoint, 'test_dir/test_file.tiff', kWriteOffset, kWriteData,
              !isReadOnly, callback);
  });

  testsToRun.push(function truncateFileShortTest() {
    var expectedFinalContent = isReadOnly ? kInitialFileContent :
                                            kFileContentAfterTruncateShort;
    // Check file content after truncate operation. The content should not
    // change on read-only file system.
    var callback = readFileAndExpectContent.bind(null,
        mountPoint, 'test_dir/test_file.tiff', expectedFinalContent,
        chrome.test.succeed);

    // Truncate should fail only on read-only file system.
    truncateFile(mountPoint, 'test_dir/test_file.tiff', kTruncateShortLength,
              !isReadOnly, callback);
  });

  testsToRun.push(function truncateFileLongTest() {
    var expectedFinalContent = isReadOnly ? kInitialFileContent :
                                            kFileContentAfterTruncateLong;
    // Check file content after truncate operation. The content should not
    // change on read-only file system.
    var callback = readFileAndExpectContent.bind(null,
        mountPoint, 'test_dir/test_file.tiff', expectedFinalContent,
        chrome.test.succeed);

    // Truncate should fail only on read-only file system.
    truncateFile(mountPoint, 'test_dir/test_file.tiff', kTruncateLongLength,
              !isReadOnly, callback);
  });

  // Skip abort tests for read-only file systems.
  if (!isReadOnly) {
    testsToRun.push(function abortWriteTest() {
      abortWriteFile(mountPoint, 'test_dir/test_file.xul.foo',
                     chrome.test.succeed);
    });

    testsToRun.push(function abortTruncateTest() {
      abortTruncateFile(mountPoint, 'test_dir/test_file.xul.foo',
                        chrome.test.succeed);
    });
  }

  testsToRun.push(function copyFileTest() {
    var verifyTarget = null;
    if (isReadOnly) {
      // If the file system is read-only, the target file should not exist after
      // copy operation.
      verifyTarget = getFile.bind(null, mountPoint, 'test_dir/subdir/copy',
          false, false, chrome.test.succeed);
    } else {
      // If the file system is not read-only, the target file should be created
      // during copy operation and its content should match the source file.
      verifyTarget = readFileAndExpectContent.bind(null, mountPoint,
          'test_dir/subdir/copy', kInitialFileContent, chrome.test.succeed);
    }

    // Verify the source file stil exists and its content hasn't changed.
    var verifySource = readFileAndExpectContent.bind(null, mountPoint,
        'test_dir/test_file.xul', kInitialFileContent, verifyTarget);

    // Copy file should fail on read-only file system.
    copyFile(mountPoint, 'test_dir/test_file.xul', 'test_dir/subdir',
        'copy', !isReadOnly, chrome.test.succeed);
  });

  testsToRun.push(function moveFileTest() {
    var verifyTarget = null;
    if (isReadOnly) {
      // If the file system is read-only, the target file should not be created
      // during move.
      verifyTarget = getFile.bind(null, mountPoint, 'test_dir/subdir/move',
          false, false, chrome.test.succeed);
    } else {
      // If the file system is read-only, the target file should be created
      // during move and its content should match the source file.
      verifyTarget = readFileAndExpectContent.bind(null, mountPoint,
          'test_dir/subdir/move', kInitialFileContent, chrome.test.succeed);
    }

    // On read-only file system the source file should still exist. Otherwise
    // the source file should have been deleted during move operation.
    var verifySource = getFile.bind(null, mountPoint,
        'test_dir/test_file.xul', false, isReadOnly, verifyTarget);

    // Copy file should fail on read-only file system.
    moveFile(mountPoint, 'test_dir/test_file.xul', 'test_dir/subdir',
        'move', !isReadOnly, chrome.test.succeed);
  });

  testsToRun.push(function deleteFileTest() {
    // Verify that file exists after delete operation if and only if the file
    // system is read only.
    var callback = getFile.bind(null, mountPoint, 'test_dir/test_file.xul.foo',
        false, isReadOnly, chrome.test.succeed);

    // Delete operation should fail for read-only file systems.
    deleteFile(mountPoint, 'test_dir/test_file.xul.foo', !isReadOnly, callback);
  });

  testsToRun.push(function deleteEmptyDirectoryTest() {
    // Verify that the directory exists after delete operation if and only if
    // the file system is read-only.
    var callback = getDirectory.bind(null, mountPoint,
        'test_dir/empty_test_dir', false, isReadOnly, chrome.test.succeed);

    // Deleting empty directory should fail for read-only file systems, and
    // succeed otherwise.
    deleteDirectory(mountPoint, 'test_dir/empty_test_dir', !isReadOnly,
                    callback);
  });

  testsToRun.push(function deleteDirectoryTest() {
    // The directory exists if and only if the file system is not drive.
    var callback = getDirectory.bind(null, mountPoint, 'test_dir', false,
        !isOnDrive, chrome.test.succeed);

    // The directory should still contain some files, so non-recursive delete
    // should fail. The drive file system does not respect is_recursive flag, so
    // the operation succeeds.
    deleteDirectory(mountPoint, 'test_dir', isOnDrive, callback);
  });

  // On drive, the directory was deleted in the previous test.
  if (!isOnDrive) {
    testsToRun.push(function deleteDirectoryRecursivelyTest() {
      // Verify that the directory exists after delete operation if and only if
      // the file system is read-only.
      var callback = getDirectory.bind(null, mountPoint, 'test_dir', false,
          isReadOnly, chrome.test.succeed);

      // Recursive delete dhouls fail only for read-only file system.
      deleteDirectoryRecursively(mountPoint, 'test_dir', !isReadOnly, callback);
    });
  }

  return testsToRun;
}

/**
 * Initializes testParams.
 * Gets test mount point and creates list of tests that should be run for it.
 *
 * @param {function(Array, string)} callback. Called with an array containing
 *     the list of the tests to run and an error message. On error list of tests
 *     to run will be null.
 */
function initTests(callback) {
  chrome.fileBrowserPrivate.requestLocalFileSystem(function(fileSystem) {
    if(!fileSystem) {
      callback(null, 'Failed to get file system.');
      return;
    }

    var possibleMountPoints = ['local/', 'restricted/', 'drive/'];

    function tryNextMountPoint() {
      if (possibleMountPoints.length == 0) {
        callback(null, 'Failed to get test mount point.');
        return;
      }

      var mountPointName = possibleMountPoints.shift();

      fileSystem.root.getDirectory(
          mountPointName, {},
          function(entry) {
            var testsToRun = collectTestsForMountPoint(mountPointName, entry);
            callback(testsToRun, 'Success.');
          },
          tryNextMountPoint);
    }

    tryNextMountPoint();
  });
}

// Trigger the tests.
initTests(function(testsToRun, errorMessage) {
  if (!testsToRun) {
    chrome.test.notifyFail('Failed to initialize tests: ' + errorMessage);
    return;
  }

  chrome.test.runTests(testsToRun);
});
