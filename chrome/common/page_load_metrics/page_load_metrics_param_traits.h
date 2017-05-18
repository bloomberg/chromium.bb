// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_PARAM_TRAITS_H_
#define CHROME_COMMON_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_PARAM_TRAITS_H_

#include "chrome/common/page_load_metrics/page_load_metrics.mojom.h"
#include "ipc/ipc_message_utils.h"

namespace IPC {

// mojom::PageLoadTiming contains child mojom structs, which are stored in
// mojo::StructPtr<>s. IPC doesn't have ParamTraits for mojo::StructPtr<>, so we
// provide custom ParamTraits for PageLoadTiming here. This is temporary while
// we run a field trial to verify there are no metric regressions due to
// migrating to mojo, and will be removed once the field trial is complete and
// shows expected results.
template <>
struct ParamTraits<page_load_metrics::mojom::PageLoadTiming> {
  typedef page_load_metrics::mojom::PageLoadTiming param_type;
  static void GetSize(base::PickleSizer* s, const param_type& p);
  static void Write(base::Pickle* m, const param_type& p);
  static bool Read(const base::Pickle* m,
                   base::PickleIterator* iter,
                   param_type* p);
  static void Log(const param_type& p, std::string* l);
};

}  // namespace IPC

#endif  // CHROME_COMMON_PAGE_LOAD_METRICS_PAGE_LOAD_METRICS_PARAM_TRAITS_H_
