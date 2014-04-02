// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_COMMON_ENVIRONMENT_DATA_H_
#define MOJO_COMMON_ENVIRONMENT_DATA_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "mojo/common/mojo_common_export.h"

namespace mojo {
namespace common {

// EnvironmentData is used to store arbitrary key/value pairs in the
// environment. The key/value pairs are owned by the Environment and deleted
// when it is deleted.
class MOJO_COMMON_EXPORT EnvironmentData {
 public:
  class MOJO_COMMON_EXPORT Data {
   public:
    Data() {}
    virtual ~Data() {}
  };

  EnvironmentData();
  ~EnvironmentData();

  static EnvironmentData* GetInstance();

  void SetData(const void* key, scoped_ptr<Data> data);

  Data* GetData(const void* key);

 private:
  typedef std::map<const void*, Data*> DataMap;

  static EnvironmentData* instance_;

  base::Lock data_lock_;

  DataMap data_map_;

  DISALLOW_COPY_AND_ASSIGN(EnvironmentData);
};

}  // namespace common
}  // namespace mojo

#endif  // MOJO_COMMON_ENVIRONMENT_DATA_H_
