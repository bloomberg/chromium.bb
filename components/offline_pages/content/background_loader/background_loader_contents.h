// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CONTENT_BACKGROUND_LOADER_BACKGROUND_LOADER_CONTENTS_H_
#define COMPONENTS_OFFLINE_PAGES_CONTENT_BACKGROUND_LOADER_BACKGROUND_LOADER_CONTENTS_H_

#include <string>

#include "base/callback.h"
#include "content/public/browser/web_contents_delegate.h"
#include "url/gurl.h"

namespace content {
class WebContentsObserver;
class WebContents;
}

namespace background_loader {

// This class maintains a WebContents used in the background. It can host
// a renderer but does not have any visible display.
class BackgroundLoaderContents : public content::WebContentsDelegate {
 public:
  // Creates BackgroundLoaderContents with specified |browser_context| and
  // |session_storage_namespace|.  This ctor should only be used if a different
  // session storage (e.g. non-persistent/incognito) should be used.
  // |session_storage_namespace| should not be null.
  BackgroundLoaderContents(
      content::BrowserContext* browser_context,
      content::SessionStorageNamespace* session_storage_namespace);

  // Creates BackgroundLoaderContents with specified |browser_context|. Uses
  // default session storage space.
  explicit BackgroundLoaderContents(content::BrowserContext* browser_context);
  ~BackgroundLoaderContents() override;

  // Loads the URL in a WebContents. Will call observe on all current observers
  // with the created WebContents.
  void LoadPage(const GURL& url);
  // Cancels loading of the current page. Calls Close() on internal WebContents.
  void Cancel();

  void AddObserver(content::WebContentsObserver* observer);
  void RemoveObserver(content::WebContentsObserver* observer);

  // content::WebContentsDelegate implementation:
  bool IsNeverVisible(content::WebContents* web_contents) override;
  void CloseContents(content::WebContents* source) override;
  bool ShouldSuppressDialogs(content::WebContents* source) override;
  bool ShouldFocusPageAfterCrash() override;
  void CanDownload(const GURL& url,
                   const std::string& request_method,
                   const base::Callback<void(bool)>& callback) override;

  bool ShouldCreateWebContents(
      content::WebContents* contents,
      int32_t route_id,
      int32_t main_frame_route_id,
      int32_t main_frame_widget_route_id,
      WindowContainerType window_container_type,
      const std::string& frame_name,
      const GURL& target_url,
      const std::string& partition_id,
      content::SessionStorageNamespace* session_storage_namespace) override;

  void AddNewContents(WebContents* source,
                      WebContents* new_contents,
                      WindowOpenDisposition disposition,
                      const gfx::Rect& initial_rect,
                      bool user_gesture,
                      bool* was_blocked) override;

#if defined(OS_ANDROID)
  bool ShouldBlockMediaRequest(const GURL& url) override;
#endif

  void RequestMediaAccessPermission(
      content::WebContents* contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) override;
  bool CheckMediaAccessPermission(content::WebContents* contents,
                                  const GURL& security_origin,
                                  content::MediaStreamType type) override;

 private:
  std::unique_ptr<content::WebContents> web_contents_;
  content::SessionStorageNamespaceMap session_storage_namespace_map_;
  content::BrowserContext* browser_context_;
  base::ObserverList<WebContentsObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundLoaderContents);
};

}  // namespace background_loader
#endif  // COMPONENTS_OFFLINE_PAGES_CONTENT_BACKGROUND_LOADER_BACKGROUND_LOADER_CONTENTS_H_
