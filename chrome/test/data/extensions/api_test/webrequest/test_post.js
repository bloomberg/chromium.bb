// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var dirName = "requestBody/";

function sendPost(formFile, parseableForm) {
  // The following variables must be updated when files in |dirName| change.
  var formData = {
      "check": ["option_A"],
      "password": ["password"],
      "radio": ["Yes"],
      "select": ["one"],
      "text\"1\u011B\u0161\u00FD\u4EBA\r\n \r\n": ["TEST_TEXT_1"],
      "text2": ["TEST_TEXT_2"],
      "text3": ["TEST_TEXT_3"],
      "txtarea": ["text\"1\u011B\u0161\u00FD\u4EBA\r\n \r\n"]
  };
  return function submitForm() {
    expect(
      [  // events
        { label: "a-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            method: "GET",
            type: "main_frame",
            url: getURL(dirName + formFile),
            frameUrl: getURL(dirName + formFile)
          }
        },
        { label: "a-onResponseStarted",
          event: "onResponseStarted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "main_frame",
            url: getURL(dirName + formFile)
          }
        },
        { label: "a-onCompleted",
          event: "onCompleted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "main_frame",
            url: getURL(dirName + formFile)
          }
        },
        { label: "s-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            method: "GET",
            type: "script",
            url: getURL("requestBody/submit.js"),
            frameUrl: getURL(dirName + formFile)
          }
        },
        { label: "s-onResponseStarted",
          event: "onResponseStarted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "script",
            url: getURL("requestBody/submit.js")
          }
        },
        { label: "s-onCompleted",
          event: "onCompleted",
          details: {
            fromCache: false,
            method: "GET",
            statusCode: 200,
            statusLine: "HTTP/1.1 200 OK",
            type: "script",
            url: getURL("requestBody/submit.js")
          }
        },
        { label: "b-onBeforeRequest",
          event: "onBeforeRequest",
          details: {
            method: "POST",
            type: "main_frame",
            url: getURL("requestBody/nonExistingTarget.html"),
            frameUrl: getURL("requestBody/nonExistingTarget.html"),
            requestBody: parseableForm ? {
              formData: formData
            } : {
              raw: [{bytes: {byteLength: 158}}]
            }
          }
        },
        { label: "b-onErrorOccurred",
          event: "onErrorOccurred",
          details: {
            error: "net::ERR_FILE_NOT_FOUND",
            fromCache: false,
            method: "POST",
            type: "main_frame",
            url: getURL("requestBody/nonExistingTarget.html")
          }
        }
      ],
      [  // event order
        ["a-onBeforeRequest", "a-onResponseStarted", "a-onCompleted",
        "s-onBeforeRequest", "s-onResponseStarted", "s-onCompleted",
        "b-onBeforeRequest", "b-onErrorOccurred"]
      ],
      {urls: ["<all_urls>"]},  // filter
      ["requestBody"]);
    navigateAndWait(getURL(dirName + formFile), function() {close();});
    close();
  }
}

runTests([
  // Navigates to a page with a form and submits it, generating a POST request.
  // First two result in url-encoded form.
  sendPost('no-enctype.html', true),
  sendPost('urlencoded.html', true),
  // Third results in multipart-encoded form.
  sendPost('multipart.html', true),
  // Fourth results in unparseable form, and thus raw data string.
  sendPost('plaintext.html', false),
]);
