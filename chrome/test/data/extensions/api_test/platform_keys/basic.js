// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var systemTokenEnabled = (location.href.indexOf("systemTokenEnabled") != -1);

var assertEq = chrome.test.assertEq;
var assertTrue = chrome.test.assertTrue;
var assertThrows = chrome.test.assertThrows;
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

function assertCertsSelected(request, expectedCerts, callback) {
  chrome.platformKeys.selectClientCertificates(
      {interactive: false, request: request},
      callbackPass(function(actualMatches) {
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

function testSelectAllCerts() {
  var requestAll = {
    certificateTypes: [],
    certificateAuthorities: []
  };
  var expectedCerts = [data.client_1];
  if (systemTokenEnabled)
    expectedCerts.push(data.client_2);
  assertCertsSelected(requestAll, expectedCerts);
}

function testSelectCA1Certs() {
  var requestCA1 = {
    certificateTypes: [],
    certificateAuthorities: [data.client_1_issuer_dn.buffer]
  };
  assertCertsSelected(requestCA1, [data.client_1]);
}

function testMatchResult() {
  var requestCA1 = {
    certificateTypes: [],
    certificateAuthorities: [data.client_1_issuer_dn.buffer]
  };
  chrome.platformKeys.selectClientCertificates(
      {interactive: false, request: requestCA1},
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

function testSignSha1() {
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

function runTests() {
  var tests = [
    testStaticMethods,
    testSelectAllCerts,
    testSelectCA1Certs,
    testMatchResult,
    testGetKeyPair,
    testSignNoHash,
    testSignSha1
  ];

  chrome.test.runTests(tests);
}

setUp(runTests);
