// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Service worker for the file upload test. It responds to a POST submission
// with an HTML document whose body describes the received form data in JSON.

function describeValue(value) {
  const result = {};
  if (value instanceof File) {
    return {type: 'file', size: value.size, name: value.name};
  } else if (value instanceof Blob) {
    return {type: 'blob', size: value.size};
  } else {
    return {type: 'string', data: value};
  }
}

async function generateResponse(request) {
  const formData = await request.formData();
  const result = {};
  result.entries = [];
  for (var pair of formData.entries()) {
    result.entries.push({key: pair[0], value: describeValue(pair[1])});
  }
  const resultString = JSON.stringify(result);
  const body = String.raw`
    <!doctype html>
    <html>
    <title>form submitted</title>
    <body>${resultString}</body>
    </html>
  `;
  const headers = {'content-type': 'text/html'};
  return new Response(body, {headers});
}

self.addEventListener('fetch', event => {
  if (event.request.method != 'POST')
    return;
  event.respondWith(generateResponse(event.request));
});
