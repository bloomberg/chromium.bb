// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_
#define CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/common/available_offline_content.mojom.h"

// Notice: this file is only included on OS_ANDROID.

// Wraps calls from the renderer thread to teh AvailableOfflineContentProvider.
class AvailableOfflineContentHelper {
 public:
  AvailableOfflineContentHelper();
  ~AvailableOfflineContentHelper();

  // Fetch available offline content and return a JSON representation.
  // Calls callback once with the return value. An empty string
  // is returned if no offline content is available.
  // Note: A call to Reset, or deletion of this object will prevent the callback
  // from running.
  void FetchAvailableContent(
      base::OnceCallback<void(const std::string& offline_content_json)>
          callback);

  // Abort previous requests and free the mojo connection.
  void Reset();

 private:
  // Binds provider_ if necessary. Returns true if the provider is bound.
  bool BindProvider();

  chrome::mojom::AvailableOfflineContentProviderPtr provider_;

  DISALLOW_COPY_AND_ASSIGN(AvailableOfflineContentHelper);
};

#endif  // CHROME_RENDERER_NET_AVAILABLE_OFFLINE_CONTENT_HELPER_H_
