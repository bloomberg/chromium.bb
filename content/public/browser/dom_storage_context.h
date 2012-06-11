// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#pragma once

#include <vector>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "webkit/dom_storage/dom_storage_context.h"

class GURL;

namespace content {

class BrowserContext;

// Represents the per-BrowserContext Local Storage data.
class DOMStorageContext {
 public:
  typedef base::Callback<
      void(const std::vector<dom_storage::DomStorageContext::UsageInfo>&)>
          GetUsageInfoCallback;

  // Returns a collection of origins using local storage to the given callback.
  virtual void GetUsageInfo(const GetUsageInfoCallback& callback) = 0;

  // Deletes the local storage data for the given origin.
  virtual void DeleteOrigin(const GURL& origin) = 0;

 protected:
  virtual ~DOMStorageContext() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
