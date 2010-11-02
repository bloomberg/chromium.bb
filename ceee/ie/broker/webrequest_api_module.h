// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The part of WebRequest implementation that needs to reside on the broker
// side.

#ifndef CEEE_IE_BROKER_WEBREQUEST_API_MODULE_H_
#define CEEE_IE_BROKER_WEBREQUEST_API_MODULE_H_

class ApiDispatcher;

namespace webrequest_api {

// Registers permanent event handlers to convert tab window handles in event
// arguments into actual tab IDs.
void RegisterInvocations(ApiDispatcher* dispatcher);

}  // namespace webrequest_api

#endif  // CEEE_IE_BROKER_WEBREQUEST_API_MODULE_H_
