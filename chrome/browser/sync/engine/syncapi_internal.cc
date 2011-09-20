// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/engine/syncapi_internal.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/util/cryptographer.h"
#include "chrome/browser/sync/protocol/password_specifics.pb.h"

using browser_sync::Cryptographer;

namespace sync_api {

sync_pb::PasswordSpecificsData* DecryptPasswordSpecifics(
    const sync_pb::EntitySpecifics& specifics, Cryptographer* crypto) {
  if (!specifics.HasExtension(sync_pb::password))
    return NULL;
  const sync_pb::PasswordSpecifics& password_specifics =
      specifics.GetExtension(sync_pb::password);
  if (!password_specifics.has_encrypted())
    return NULL;
  const sync_pb::EncryptedData& encrypted = password_specifics.encrypted();
  scoped_ptr<sync_pb::PasswordSpecificsData> data(
      new sync_pb::PasswordSpecificsData);
  if (!crypto->Decrypt(encrypted, data.get()))
    return NULL;
  return data.release();
}

// The list of names which are reserved for use by the server.
static const char* kForbiddenServerNames[] = { "", ".", ".." };

// When taking a name from the syncapi, append a space if it matches the
// pattern of a server-illegal name followed by zero or more spaces.
void SyncAPINameToServerName(const std::string& sync_api_name,
                             std::string* out) {
  *out = sync_api_name;
  if (IsNameServerIllegalAfterTrimming(*out))
    out->append(" ");
}

// Checks whether |name| is a server-illegal name followed by zero or more space
// characters.  The three server-illegal names are the empty string, dot, and
// dot-dot.  Very long names (>255 bytes in UTF-8 Normalization Form C) are
// also illegal, but are not considered here.
bool IsNameServerIllegalAfterTrimming(const std::string& name) {
  size_t untrimmed_count = name.find_last_not_of(' ') + 1;
  for (size_t i = 0; i < arraysize(kForbiddenServerNames); ++i) {
    if (name.compare(0, untrimmed_count, kForbiddenServerNames[i]) == 0)
      return true;
  }
  return false;
}

// Compare the values of two EntitySpecifics, accounting for encryption.
bool AreSpecificsEqual(const browser_sync::Cryptographer* cryptographer,
                       const sync_pb::EntitySpecifics& left,
                       const sync_pb::EntitySpecifics& right) {
  // Note that we can't compare encrypted strings directly as they are seeded
  // with a random value.
  std::string left_plaintext, right_plaintext;
  if (left.has_encrypted()) {
    if (!cryptographer->CanDecrypt(left.encrypted())) {
      NOTREACHED() << "Attempting to compare undecryptable data.";
      return false;
    }
    left_plaintext = cryptographer->DecryptToString(left.encrypted());
  } else {
    left_plaintext = left.SerializeAsString();
  }
  if (right.has_encrypted()) {
    if (!cryptographer->CanDecrypt(right.encrypted())) {
      NOTREACHED() << "Attempting to compare undecryptable data.";
      return false;
    }
    right_plaintext = cryptographer->DecryptToString(right.encrypted());
  } else {
    right_plaintext = right.SerializeAsString();
  }
  if (left_plaintext == right_plaintext) {
    return true;
  }
  return false;
}

} // namespace sync_api
