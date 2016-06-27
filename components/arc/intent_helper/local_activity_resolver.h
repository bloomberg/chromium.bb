// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_HELPER_H_
#define COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_HELPER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/arc/common/intent_helper.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"

class GURL;

namespace arc {

class LocalActivityResolver : public base::RefCounted<LocalActivityResolver> {
 public:
  LocalActivityResolver();

  bool ShouldChromeHandleUrl(const GURL& url);
  void UpdateIntentFilters(mojo::Array<mojom::IntentFilterPtr> intent_filters);

 private:
  friend class base::RefCounted<LocalActivityResolver>;
  ~LocalActivityResolver();

  bool IsRelevantIntentFilter(const mojom::IntentFilterPtr& intent_filter);
  bool FilterHasViewAction(const mojom::IntentFilterPtr& intent_filter);
  bool FilterCategoryIsBrowsable(const mojom::IntentFilterPtr& intent_filter);
  bool FilterHandlesWebSchemes(const mojom::IntentFilterPtr& intent_filter);

  // List of intent filters from Android. Used to determine if Chrome should
  // handle a URL without handing off to Android.
  mojo::Array<mojom::IntentFilterPtr> intent_filters_;

  DISALLOW_COPY_AND_ASSIGN(LocalActivityResolver);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_INTENT_HELPER_LOCAL_ACTIVITY_HELPER_H_
