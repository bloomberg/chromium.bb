// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CONSTS_H_
#define CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CONSTS_H_

namespace content {

constexpr char kAcceptHeaderSignedExchangeSuffix[] =
    ",application/signed-exchange;v=b0";

// Field names defined in the application/http-exchange+cbor content type:
// https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html#rfc.section.5

constexpr char kCertSha256Key[] = "certSha256";
constexpr char kDateKey[] = "date";
constexpr char kExpiresKey[] = "expires";
constexpr char kHeadersKey[] = "headers";
constexpr char kHtxg[] = "htxg";
constexpr char kMethodKey[] = ":method";
constexpr char kPayload[] = "payload";
constexpr char kRequest[] = "request";
constexpr char kResponse[] = "response";
constexpr char kSignature[] = "signature";
constexpr char kStatusKey[] = ":status";
constexpr char kUrlKey[] = ":url";
constexpr char kValidityUrlKey[] = "validityUrl";

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_PACKAGE_SIGNED_EXCHANGE_CONSTS_H_
