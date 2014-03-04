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
  // Must be kept in sync with blink::WebMediaPlayerClient::MediaKeyErrorCode
  // (enforced in webmediaplayer_impl.cc).
  enum KeyError {
    kUnknownError = 1,
    kClientError,
    // The commented v0.1b values below have never been used.
    // kServiceError,
    kOutputError = 4,
    // kHardwareChangeError,
    // kDomainError,
    kMaxKeyError  // Must be last and greater than any legit value.
  };

  const static uint32 kInvalidSessionId = 0;

  MediaKeys();
  virtual ~MediaKeys();

  // Creates a session with the |content_type| and |init_data| provided.
  // Returns true if a session is successfully created, false otherwise.
  // Note: UpdateSession() and ReleaseSession() should only be called after
  // SessionCreatedCB is fired.
  // TODO(jrummell): Remove return value when prefixed API is removed.
  // See http://crbug.com/342510
  virtual bool CreateSession(uint32 session_id,
                             const std::string& content_type,
                             const uint8* init_data,
                             int init_data_length) = 0;

  // Loads a session with the |web_session_id| provided.
  // Note: UpdateSession() and ReleaseSession() should only be called after
  // SessionCreatedCB is fired.
  virtual void LoadSession(uint32 session_id,
                           const std::string& web_session_id) = 0;

  // Updates a session specified by |session_id| with |response|.
  virtual void UpdateSession(uint32 session_id,
                             const uint8* response,
                             int response_length) = 0;

  // Releases the session specified by |session_id|.
  virtual void ReleaseSession(uint32 session_id) = 0;

  // Gets the Decryptor object associated with the MediaKeys. Returns NULL if
  // no Decryptor object is associated. The returned object is only guaranteed
  // to be valid during the MediaKeys' lifetime.
  virtual Decryptor* GetDecryptor();

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaKeys);
};

// Key event callbacks. See the spec for details:
// https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#event-summary
typedef base::Callback<
    void(uint32 session_id, const std::string& web_session_id)>
    SessionCreatedCB;

typedef base::Callback<void(uint32 session_id,
                            const std::vector<uint8>& message,
                            const std::string& destination_url)>
    SessionMessageCB;

typedef base::Callback<void(uint32 session_id)> SessionReadyCB;

typedef base::Callback<void(uint32 session_id)> SessionClosedCB;

typedef base::Callback<void(uint32 session_id,
                            media::MediaKeys::KeyError error_code,
                            uint32 system_code)> SessionErrorCB;

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_KEYS_H_
