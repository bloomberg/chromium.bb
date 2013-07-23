// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_FAKE_SESSION_ACCESSOR_H_
#define CHROME_TEST_CHROMEDRIVER_FAKE_SESSION_ACCESSOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/test/chromedriver/session.h"

namespace base {
class AutoLock;
}

// Fake session accessor that doesn't actually lock.
class FakeSessionAccessor : public SessionAccessor {
 public:
  explicit FakeSessionAccessor(Session* session);

  bool IsSessionDeleted() const;

  // Overridden from SessionAccessor:
  virtual Session* Access(scoped_ptr<base::AutoLock>* lock) OVERRIDE;
  virtual void DeleteSession() OVERRIDE;

 private:
  virtual ~FakeSessionAccessor();

  Session* session_;
  bool is_accessed_;
  bool is_session_deleted_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_FAKE_SESSION_ACCESSOR_H_
