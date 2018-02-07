// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_CONSTS_H_
#define CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_CONSTS_H_

namespace content {

// Field names defined in the application/http-exchange+cbor content type:
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#rfc.section.5
constexpr char kHtxg[] = "htxg";
constexpr char kRequest[] = "request";
constexpr char kResponse[] = "response";
constexpr char kPayload[] = "payload";
constexpr char kUrlKey[] = ":url";
constexpr char kMethodKey[] = ":method";
constexpr char kStatusKey[] = ":status";

}  // namespace content

#endif  // CONTENT_BROWSER_LOADER_SIGNED_EXCHANGE_CONSTS_H_
