// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_
#define CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_

#include "base/memory/scoped_ptr.h"
#include "content/browser/background_sync/background_sync.pb.h"
#include "content/browser/background_sync/background_sync_registration_options.h"
#include "content/common/background_sync_service.mojom.h"
#include "content/common/content_export.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/type_converter.h"

namespace content {

class CONTENT_EXPORT BackgroundSyncRegistration {
 public:
  using RegistrationId = int64_t;
  static const RegistrationId kInitialId;

  BackgroundSyncRegistration();
  bool Equals(const BackgroundSyncRegistration& other) const;
  bool IsValid() const;

  const BackgroundSyncRegistrationOptions* options() const { return &options_; }
  BackgroundSyncRegistrationOptions* options() { return &options_; }

  RegistrationId id() const { return id_; }
  void set_id(RegistrationId id) { id_ = id; }

  SyncState sync_state() const { return sync_state_; }
  void set_sync_state(SyncState state) { sync_state_ = state; }

 private:
  static const RegistrationId kInvalidRegistrationId;

  BackgroundSyncRegistrationOptions options_;
  RegistrationId id_ = kInvalidRegistrationId;
  SyncState sync_state_ = SYNC_STATE_PENDING;
};

}  // namespace content

namespace mojo {

template <>
struct CONTENT_EXPORT
    TypeConverter<scoped_ptr<content::BackgroundSyncRegistration>,
                  content::SyncRegistrationPtr> {
  static scoped_ptr<content::BackgroundSyncRegistration> Convert(
      const content::SyncRegistrationPtr& input);
};

template <>
struct CONTENT_EXPORT TypeConverter<content::SyncRegistrationPtr,
                                    content::BackgroundSyncRegistration> {
  static content::SyncRegistrationPtr Convert(
      const content::BackgroundSyncRegistration& input);
};

}  // namespace

#endif  // CONTENT_BROWSER_BACKGROUND_SYNC_BACKGROUND_SYNC_REGISTRATION_H_
