// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_INTERNAL_H_
#define CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_INTERNAL_H_

// The functions defined are shared among some of the classes that implement
// the internal sync_api.  They are not to be used by clients of the API.

#include <string>

namespace browser_sync {
class Cryptographer;
}

namespace sync_pb {
class EntitySpecifics;
class PasswordSpecificsData;
}

namespace sync_api {
sync_pb::PasswordSpecificsData* DecryptPasswordSpecifics(
    const sync_pb::EntitySpecifics& specifics,
    browser_sync::Cryptographer* crypto);

void SyncAPINameToServerName(const std::string& sync_api_name,
                             std::string* out);

bool IsNameServerIllegalAfterTrimming(const std::string& name);

bool AreSpecificsEqual(const browser_sync::Cryptographer* cryptographer,
                       const sync_pb::EntitySpecifics& left,
                       const sync_pb::EntitySpecifics& right);
}

#endif  // CHROME_BROWSER_SYNC_ENGINE_SYNCAPI_INTERNAL_H_
