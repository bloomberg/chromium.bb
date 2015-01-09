// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_CDM_PROMISE_ADAPTER_H_
#define MEDIA_BASE_CDM_PROMISE_ADAPTER_H_

#include "base/basictypes.h"
#include "base/containers/scoped_ptr_hash_map.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/cdm_promise.h"
#include "media/base/media_export.h"

namespace media {

// Helps convert CdmPromises to an integer identifier and vice versa. The
// integer identifier is needed where we cannot pass CdmPromises through, such
// as PPAPI, IPC and JNI.
class MEDIA_EXPORT CdmPromiseAdapter {
 public:
  CdmPromiseAdapter();
  ~CdmPromiseAdapter();

  // Takes ownership of |promise| and returns an integer promise ID.
  uint32_t SavePromise(scoped_ptr<media::CdmPromise> promise);

  // Finds, takes the ownership of and returns the promise for |promise_id|.
  // Returns null if no promise can be found.
  scoped_ptr<CdmPromise> TakePromise(uint32_t promise_id);

  // Rejects and clears all |promises_|.
  void Clear();

 private:
  // A map between promise IDs and CdmPromises. It owns the CdmPromises.
  typedef base::ScopedPtrHashMap<uint32_t, CdmPromise> PromiseMap;

  uint32_t next_promise_id_;
  PromiseMap promises_;

  DISALLOW_COPY_AND_ASSIGN(CdmPromiseAdapter);
};

}  // namespace media

#endif  // MEDIA_BASE_CDM_PROMISE_ADAPTER_H_
