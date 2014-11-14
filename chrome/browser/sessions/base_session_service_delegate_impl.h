// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_DELEGATE_IMPL_H_

#include "components/sessions/base_session_service_delegate.h"

class BaseSessionServiceDelegateImpl
    : public sessions::BaseSessionServiceDelegate {
 public:
  // If |use_delayed_save| is true, save operations can be performed as a
  // delayed task. Generally |used_delayed_save| should be true, testing may
  // supply false.
  explicit BaseSessionServiceDelegateImpl(bool should_use_delayed_save);
  ~BaseSessionServiceDelegateImpl() override {}

  // BaseSessionServiceDelegate:
  base::SequencedWorkerPool* GetBlockingPool() override;
  bool ShouldTrackEntry(const GURL& url) override;
  bool ShouldUseDelayedSave() override;
  void OnWillSaveCommands() override {}
  void OnSavedCommands() override {}

 private:
  // True if save operations can be performed as a delayed task. This can be
  // disabled for unit tests.
  const bool should_use_delayed_save_;

  DISALLOW_COPY_AND_ASSIGN(BaseSessionServiceDelegateImpl);
};

#endif  // CHROME_BROWSER_SESSIONS_BASE_SESSION_SERVICE_DELEGATE_IMPL_H_
