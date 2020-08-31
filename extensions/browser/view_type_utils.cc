// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/view_type_utils.h"

#include "base/lazy_instance.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extensions_browser_client.h"

using content::WebContents;

namespace extensions {

namespace {

const char kViewTypeUserDataKey[] = "ViewTypeUserData";

class ViewTypeUserData : public base::SupportsUserData::Data {
 public:
  explicit ViewTypeUserData(ViewType type) : type_(type) {}
  ~ViewTypeUserData() override {}
  ViewType type() { return type_; }

 private:
  ViewType type_;
};

}  // namespace

ViewType GetViewType(WebContents* tab) {
  if (!tab)
    return VIEW_TYPE_INVALID;

  ViewTypeUserData* user_data = static_cast<ViewTypeUserData*>(
      tab->GetUserData(&kViewTypeUserDataKey));

  return user_data ? user_data->type() : VIEW_TYPE_INVALID;
}

void SetViewType(WebContents* tab, ViewType type) {
  tab->SetUserData(&kViewTypeUserDataKey,
                   std::make_unique<ViewTypeUserData>(type));

  ExtensionsBrowserClient::Get()->AttachExtensionTaskManagerTag(tab, type);
}

}  // namespace extensions
