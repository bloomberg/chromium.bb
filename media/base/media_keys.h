// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_KEYS_H_
#define MEDIA_BASE_MEDIA_KEYS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/media_export.h"

namespace media {

class Decryptor;

// Performs media key operations.
//
// All key operations are called on the renderer thread. Therefore, these calls
// should be fast and nonblocking; key events should be fired asynchronously.
class MEDIA_EXPORT MediaKeys {
 public:
  // Reported to UMA, so never reuse a value!
  // Must be kept in sync with WebKit::WebMediaPlayerClient::MediaKeyErrorCode
  // (enforced in webmediaplayer_impl.cc).
  enum KeyError {
    kUnknownError = 1,
    kClientError,
    // The following v0.1b values have never been used.
    // kServiceError,
    // kOutputError,
    // kHardwareChangeError,
    // kDomainError,
    kMaxKeyError  // Must be last and greater than any legit value.
  };

  MediaKeys();
  virtual ~MediaKeys();

  // Generates a key request with the |type| and |init_data| provided.
  // Returns true if generating key request succeeded, false otherwise.
  // Note: AddKey() and CancelKeyRequest() should only be called after
  // GenerateKeyRequest() returns true.
  virtual bool GenerateKeyRequest(const std::string& type,
                                  const uint8* init_data,
                                  int init_data_length) = 0;

  // Adds a |key| to the session. The |key| is not limited to a decryption
  // key. It can be any data that the key system accepts, such as a license.
  // If multiple calls of this function set different keys for the same
  // key ID, the older key will be replaced by the newer key.
  virtual void AddKey(const uint8* key, int key_length,
                      const uint8* init_data, int init_data_length,
                      const std::string& session_id) = 0;

  // Cancels the key request specified by |session_id|.
  virtual void CancelKeyRequest(const std::string& session_id) = 0;

  // Gets the Decryptor object associated with the MediaKeys. Returns NULL if
  // no Decryptor object is associated. The returned object is only guaranteed
  // to be valid during the MediaKeys' lifetime.
  virtual Decryptor* GetDecryptor();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaKeys);
};

// Key event callbacks. See the spec for details:
// http://dvcs.w3.org/hg/html-media/raw-file/eme-v0.1b/encrypted-media/encrypted-media.html#event-summary
typedef base::Callback<void(const std::string& session_id)> KeyAddedCB;

typedef base::Callback<void(const std::string& session_id,
                            media::MediaKeys::KeyError error_code,
                            int system_code)> KeyErrorCB;

typedef base::Callback<void(const std::string& session_id,
                            const std::vector<uint8>& message,
                            const std::string& default_url)> KeyMessageCB;

typedef base::Callback<void(const std::string& session_id,
                            const std::string& type,
                            scoped_ptr<uint8[]> init_data,
                            int init_data_size)> NeedKeyCB;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_KEYS_H_
