// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_
#define COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"

namespace safe_browsing {

class V4Store;

// Factory for creating V4Store. Tests implement this factory to create fake
// stores for testing.
class V4StoreFactory {
 public:
  virtual ~V4StoreFactory() {}
  virtual V4Store* CreateV4Store(
      const scoped_refptr<base::SequencedTaskRunner>& task_runner,
      const base::FilePath& store_path);
};

class V4Store {
 public:
  // The |task_runner| is used to ensure that the operations in this file are
  // performed on the correct thread. |store_path| specifies the location on
  // disk for this file.
  V4Store(const scoped_refptr<base::SequencedTaskRunner>& task_runner,
          const base::FilePath& store_path);
  virtual ~V4Store();

  // Reset internal state and delete the backing file.
  virtual bool Reset();

  const base::FilePath& store_path() const {
    return store_path_;
  }

 protected:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  const base::FilePath store_path_;

  DISALLOW_COPY_AND_ASSIGN(V4Store);
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_V4_STORE_H_
