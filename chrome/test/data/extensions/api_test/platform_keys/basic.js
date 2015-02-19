// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var systemTokenEnabled = (location.search.indexOf("systemTokenEnabled") != -1);
var selectedTestSuite = location.hash.slice(1);
console.log('[SELECTED TEST SUITE] ' + selectedTestSuite +
            ', systemTokenEnable ' + systemTokenEnabled);

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var fail = chrome.test.fail;
var succeed = chrome.test.succeed;
var callbackPass = chrome.test.callbackPass;
var callbackFail= chrome.test.callbackFail;

// Each value is the path to a file in this extension's folder that will be
// loaded and replaced by a Uint8Array in the setUp() function below.
var data = {
  // X.509 client certificates in DER encoding.
  // openssl x509 -in net/data/ssl/certificates/client_1.pem -outform DER -out
  //   client_1.der
  client_1: 'client_1.der',

  // openssl x509 -in net/data/ssl/certificates/client_2.pem -outform DER -out
  //   client_2.der
  client_2: 'client_2.der',

  // The public key of client_1 as Subject Public Key Info in DER encoding.
  // openssl rsa -in net/data/ssl/certificates/client_1.key -inform PEM -out
  //   pubkey.der -pubout -outform DER
  client_1_spki: 'client_1_spki.der',

  // The distinguished name of the CA that issued client_1 in DER encoding.
  // openssl asn1parse -in client_1.der -inform DER -strparse 32 -out
  //   client_1_issuer_dn.der
  client_1_issuer_dn: 'client_1_issuer_dn.der',

  // echo -n "hello world" > data
  raw_data: 'data',

  // openssl rsautl -inkey net/data/ssl/certificates/client_1.key -sign -in
  //   data -pkcs -out signature_nohash_pkcs
  signature_nohash_pkcs: 'signature_nohash_pkcs',

  // openssl dgst -sha1 -sign net/data/ssl/certificates/client_1.key
  //   -out signature_sha1_pkcs data
  signature_sha1_pkcs: 'signature_sha1_pkcs',
};

// Reads the binary file at |path| and passes it as a Uin8Array to |callback|.
function readFile(path, callback) {
  var oReq = new XMLHttpRequest();
  oReq.responseType = "arraybuffer";
  oReq.open("GET", path, true /* asynchronous */);
  oReq.onload = function() {
    var arrayBuffer = oReq.response;
    if (arrayBuffer) {
      callback(new Uint8Array(arrayBuffer));
    } else {
      callback(null);
    }
  };
  oReq.send(null);
}

// For each key in dictionary, replaces the path dictionary[key] by the content
// of the resource located at that path stored in a Uint8Array.
function readData(dictionary, callback) {
  var keys = Object.keys(dictionary);
  function recurse(index) {
    if (index >= keys.length) {
      callback();
      return;
    }
    var key = keys[index];
    var path = dictionary[key];
    readFile(path, function(array) {
      assertTrue(!!array);
      dictionary[key] = array;
      recurse(index + 1);
    });
  }

  recurse(0);
}

function setUp(callback) {
  readData(data, callback);
}

// Some array comparison. Note: not lexicographical!
function compareArrays(array1, array2) {
  if (array1.length < array2.length)
    return -1;
  if (array1.length > array2.length)
    return 1;
  for (var i = 0; i < array1.length; i++) {
    if (array1[i] < array2[i])
      return -1;
    if (array1[i] > array2[i])
      return 1;
  }
  return 0;
}

/**
 * @param {ArrayBufferView[]} certs
 * @return {ArrayBufferView[]} |certs| sorted in some order.
 */
function sortCerts(certs) {
  return certs.sort(compareArrays);
}

function assertCertsSelected(details, expectedCerts, callback) {
  chrome.platformKeys.selectClientCertificates(
      details, callbackPass(function(actualMatches) {
        assertEq(expectedCerts.length, actualMatches.length,
                 'Number of stored certs not as expected');
        if (expectedCerts.length == actualMatches.length) {
          var actualCerts = actualMatches.map(function(match) {
            return new Uint8Array(match.certificate);
          });
          actualCerts = sortCerts(actualCerts);
          expectedCerts = sortCerts(expectedCerts);
          for (var i = 0; i < expectedCerts.length; i++) {
            assertEq(expectedCerts[i], actualCerts[i],
                     'Certs at index ' + i + ' differ');
          }
        }
        if (callback)
          callback();
      }));
}

function checkAlgorithmIsCopiedOnRead(key) {
  var algorithm = key.algorithm;
  var originalAlgorithm = {
    name: algorithm.name,
    modulusLength: algorithm.modulusLength,
    publicExponent: algorithm.publicExponent,
    hash: {name: algorithm.hash.name}
  };
  var originalModulusLength = algorithm.modulusLength;
  algorithm.hash.name = null;
  algorithm.hash = null;
  algorithm.name = null;
  algorithm.modulusLength = null;
  algorithm.publicExponent = null;
  assertEq(originalAlgorithm, key.algorithm);
}

function checkPropertyIsReadOnly(object, key) {
  var original = object[key];
  try {
    object[key] = {};
    fail('Expected the property to be read-only and an exception to be thrown');
  } catch (error) {
    assertEq(original, object[key]);
  }
}

function checkPrivateKeyFormat(privateKey) {
  assertEq('private', privateKey.type);
  assertEq(false, privateKey.extractable);
  checkPropertyIsReadOnly(privateKey, 'algorithm');
  checkAlgorithmIsCopiedOnRead(privateKey);
}

function checkPublicKeyFormat(publicKey) {
  assertEq('public', publicKey.type);
  assertEq(true, publicKey.extractable);
  checkPropertyIsReadOnly(publicKey, 'algorithm');
  checkAlgorithmIsCopiedOnRead(publicKey);
}

function testStaticMethods() {
  assertTrue(!!chrome.platformKeys, "No platformKeys namespace.");
  assertTrue(!!chrome.platformKeys.selectClientCertificates,
             "No selectClientCertificates function.");
  succeed();
}

function testHasSubtleCryptoMethods(token) {
  assertTrue(!!token.subtleCrypto.generateKey,
             "token has no generateKey method");
  assertTrue(!!token.subtleCrypto.sign, "token has no sign method");
  assertTrue(!!token.subtleCrypto.exportKey, "token has no exportKey method");
  succeed();
}

var requestAll = {
  certificateTypes: [],
  certificateAuthorities: []
};

// Depends on |data|, thus it cannot be created immediately.
function requestCA1() {
  return {
    certificateTypes: [],
    certificateAuthorities: [data.client_1_issuer_dn.buffer]
  };
}

function testSelectAllCerts() {
  var expectedCerts = [data.client_1];
  if (systemTokenEnabled)
    expectedCerts.push(data.client_2);
  assertCertsSelected({interactive: false, request: requestAll}, expectedCerts);
}

function testSelectCA1Certs() {
  assertCertsSelected({interactive: false, request: requestCA1()},
                      [data.client_1]);
}

function testSelectAllReturnsNoCerts() {
  assertCertsSelected({interactive: false, request: requestAll},
                      [] /* no certs selected */);
}

function testSelectAllReturnsClient1() {
  assertCertsSelected({interactive: false, request: requestAll},
                      [data.client_1]);
}

function testInteractiveSelectNoCerts() {
  assertCertsSelected({interactive: true, request: requestAll},
                      [] /* no certs selected */);
}

function testInteractiveSelectClient1() {
  assertCertsSelected({interactive: true, request: requestAll},
                      [data.client_1]);
}

function testMatchResult() {
  chrome.platformKeys.selectClientCertificates(
      {interactive: false, request: requestCA1()},
      callbackPass(function(matches) {
        var expectedAlgorithm = {
          modulusLength: 2048,
          name: "RSASSA-PKCS1-v1_5",
          publicExponent: new Uint8Array([0x01, 0x00, 0x01])
        };
        var actualAlgorithm = matches[0].keyAlgorithm;
        assertEq(
            expectedAlgorithm, actualAlgorithm,
            'Member algorithm of Match does not equal the expected algorithm');
      }));
}

function testGetKeyPair() {
  var keyParams = {
    // Algorithm names are case-insensitive.
    'hash': {'name': 'sha-1'}
  };
  chrome.platformKeys.getKeyPair(
      data.client_1.buffer, keyParams,
      callbackPass(function(publicKey, privateKey) {
        var expectedAlgorithm = {
          modulusLength: 2048,
          name: "RSASSA-PKCS1-v1_5",
          publicExponent: new Uint8Array([0x01, 0x00, 0x01]),
          hash: {name: 'SHA-1'}
        };
        assertEq(expectedAlgorithm, publicKey.algorithm);
        assertEq(expectedAlgorithm, privateKey.algorithm);

        checkPublicKeyFormat(publicKey);
        checkPrivateKeyFormat(privateKey);

        chrome.platformKeys.subtleCrypto()
            .exportKey('spki', publicKey)
            .then(callbackPass(function(actualPublicKeySpki) {
              assertTrue(
                  compareArrays(data.client_1_spki, actualPublicKeySpki) == 0,
                  'Match did not contain correct public key');
            }),
                  function(error) { fail("Export failed: " + error); });
      }));
}

function testSignNoHash() {
  var keyParams = {
    // Algorithm names are case-insensitive.
    hash: {name: 'NONE'}
  };
  var signParams = {
    name: 'RSASSA-PKCS1-v1_5'
  };
  chrome.platformKeys.getKeyPair(
      data.client_1.buffer, keyParams,
      callbackPass(function(publicKey, privateKey) {
        chrome.platformKeys.subtleCrypto()
            .sign(signParams, privateKey, data.raw_data)
            .then(callbackPass(function(signature) {
              var actualSignature = new Uint8Array(signature);
              assertTrue(compareArrays(data.signature_nohash_pkcs,
                                       actualSignature) == 0,
                         'Incorrect signature');
            }));
      }));
}

function testSignSha1Client1() {
  var keyParams = {
    // Algorithm names are case-insensitive.
    hash: {name: 'Sha-1'}
  };
  var signParams = {
    // Algorithm names are case-insensitive.
    name: 'RSASSA-Pkcs1-v1_5'
  };
  chrome.platformKeys.getKeyPair(
      data.client_1.buffer, keyParams,
      callbackPass(function(publicKey, privateKey) {
        chrome.platformKeys.subtleCrypto()
            .sign(signParams, privateKey, data.raw_data)
            .then(callbackPass(function(signature) {
              var actualSignature = new Uint8Array(signature);
              assertTrue(
                  compareArrays(data.signature_sha1_pkcs, actualSignature) == 0,
                  'Incorrect signature');
            }));
      }));
}

// TODO(pneubeck): Test this by verifying that no private key is returned, once
// that's implemented.
function testSignFails(cert) {
  var keyParams = {
    hash: {name: 'SHA-1'}
  };
  var signParams = {
    name: 'RSASSA-PKCS1-v1_5'
  };
  chrome.platformKeys.getKeyPair(
      cert.buffer, keyParams, callbackPass(function(publicKey, privateKey) {
        chrome.platformKeys.subtleCrypto()
            .sign(signParams, privateKey, data.raw_data)
            .then(function(signature) { fail('sign was expected to fail.'); },
                  callbackPass(function(error) {
                    assertTrue(error instanceof Error);
                    assertEq(
                        'The operation failed for an operation-specific reason',
                        error.message);
                  }));
      }));
}

function testSignClient1Fails() {
  testSignFails(data.client_1);
}

function testSignClient2Fails() {
  testSignFails(data.client_2);
}

var testSuites = {
  // These tests assume already granted permissions for client_1 and client_2.
  // On interactive selectClientCertificates calls, the simulated user does not
  // select any cert.
  basicTests: function() {
    var tests = [
      testStaticMethods,
      testSelectAllCerts,
      testSelectCA1Certs,
      testInteractiveSelectNoCerts,
      testMatchResult,
      testGetKeyPair,
      testSignNoHash,
      testSignSha1Client1,
    ];

    chrome.test.runTests(tests);
  },

  // This test suite starts without any granted permissions.
  // On interactive selectClientCertificates calls, the simulated user selects
  // client_1, if matching.
  permissionTests: function() {
    var tests = [
      // Without permissions both sign attempts fail.
      testSignClient1Fails,
      testSignClient2Fails,

      // Without permissions, non-interactive select calls return no certs.
      testSelectAllReturnsNoCerts,

      testInteractiveSelectClient1,
      // Now that the permission for client_1 is granted.

      // Verify that signing with client_1 is possible and with client_2 still
      // fails.
      testSignSha1Client1,
      testSignClient2Fails,

      // Verify that client_1 can still be selected interactively.
      testInteractiveSelectClient1,

      // Verify that client_1 but not client_2 is selected in non-interactive
      // calls.
      testSelectAllReturnsClient1,
    ];

    chrome.test.runTests(tests);
  }
};

setUp(testSuites[selectedTestSuite]);
