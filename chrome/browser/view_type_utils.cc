// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/view_type_utils.h"

#include "base/lazy_instance.h"
#include "base/property_bag.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace chrome {

static base::LazyInstance<base::PropertyAccessor<ViewType> >
    g_view_type_property_accessor = LAZY_INSTANCE_INITIALIZER;

base::PropertyAccessor<ViewType>* GetPropertyAccessor() {
  return g_view_type_property_accessor.Pointer();
}

ViewType GetViewType(WebContents* tab) {
  if (!tab)
    return VIEW_TYPE_INVALID;
  ViewType* type = GetPropertyAccessor()->GetProperty(tab->GetPropertyBag());
  if (type)
    return *type;
  return VIEW_TYPE_INVALID;
}

void SetViewType(WebContents* tab, ViewType type) {
  GetPropertyAccessor()->SetProperty(tab->GetPropertyBag(), type);
}

}  // namespace chrome
